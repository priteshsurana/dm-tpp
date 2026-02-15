#pragma once

#include "result.h"
#include "error.h"
#include <string>
#include <optional>
#include <libpq-fe.h>
#include <memory>
#include <vector>
#include <mutex>
#include <functional>
#include <condition_variable>
#include <chrono>

namespace dmtpp::persistence {
    // Forward declaration
    class Connection;
    class ConnectionPool;

/*
RAII wrapper for a single PostgreSQL connection

Usage;
    auto conn_result = pool.acquire();
    if (!conn_result) return conn_result.error();

    auto conn = conn_result.value();
    auto result = conn->execute("SELECT 1");
    // Connectoin automatically released when conn goes out of scope
*/

    class Connection {
        explicit Connection(PGconn* conn, ConnectionPool* pool);
        ~Connection();

        // Disable copy, enable move
        Connection(const Connection&) = delete;
        Connection* operator=(const Connection&) = delete;
        Connection(Connection&&) noexcept;
        Connection& operator=(Connection&&) noexcept;

        // Execute a simple query (no parameters)
        core::Result<PGresult*, core::Error> execute(const std::string& query);

        // Execute parameterized query (SQL injection safe)
        // Example: execute_params("SELECT * FROM txns WHERE id = $1", {"tx-123"})
        core::Result<PGresult*, core::Error> execute_params(
            const std::string& query,
            const std::vector<std::string>& params
        );

        // Transaction control
        core::Result<void, core::Error> begin();
        core::Result<void, core::Error> commit();
        core::Result<void, core::Error> rollback();

        // Health check
        bool is_healthy() const;

        // Get raw connection (use sparingly)
        PGconn* raw() {return conn_;}

        private:
        PGconn* conn_;
        ConnectionPool* pool_; //For returning connection on destruction
        bool in_transaction_ = false; //Track transaction state for safety

        core::Error pg_error_to_error(const std::string& operation) const;

    };


    /*
    Connection pool for PostgreSQL

    Thread-safe. Manage a pool of connections to avoid connection overhead.
    Automatically heandles connection failures and retry logic.

    Usage:
        PostgresClient client("host=localhost dbname=dmtpp")
        auto init_result = client.initialize(10); // max 10 connections
        if (!init_result) {
            std::cerr << "Failed to initialize connection pool: " << init_result.error().to_string() << std::endl;
            return;
        }

        // Acquire connection from the pool (RAII - auto-released)
        auto conn = client.acquire_connection();
        if (!conn) {
            return conn.error();
        }
        
        Use connection
        auto result = conn.value()->execute("SELECT 1");
    */

    class PostgresClient {
        public:
        explicit PostgresClient(const std::string& connection_string);
        ~PostgresClient();

        // Initialize the connection pool
        // pool_size: maximum number of connections in the pool
        core::Result<void, core::Error> initialize(size_t pool_size = 10);

        // Acquire a connection from the pool (RAII wrapper)
        // Blocks if all connections are in use(with timeout)
        core::Result<std::unique_ptr<Connection>, core::Error> acquire_connection(
            std::chrono::milliseconds timeout = std::chrono::milliseconds(5000)
        );

        // Execute a query with automatic connection management
        // Convenience method - acquires connection, executes query, releases connection
        core::Result<PGresult*, core::Error> execute(const std::string& query);

        // Execute parameterized query wih automatic connection management
        core::Result<PGresult*, core::Error> execute_params(
            const std::string& query,
            const std::vector<std::string>& params
        );

        // Execute a transaction(begin execute function commit rollback)
        // The function receives a connection* and must return Result<T, Error>
        template<typename T>
        core::Result<T, core::Error> execute_transaction(
            std::function<core::Result<T, core::Error>(Connection*)> transaction_fn
        );

        // Health check
        bool is_healthy() const;

        // Stats
        size_t pool_size() const;
        size_t available_connections() const;

        private:
        friend class Connection; // Allow Connection to access private members for returning to pool

        std::string connection_string_;
        std::vector<PGconn*> pool_;
        std::vector<bool> in_use_; // Track which connections are in use
        mutable std::mutex mutex_;
        mutable std::condition_variable cond_var_;
        bool initialized_ = false;

        void release_connection(PGconn* conn);
        core::Result<PGconn*, core::Error> create_connection();
    };

    // Template implementation for execute_transaction
    template<typename T>
    core::Result<T, core::Error> PostgresClient::execute_transaction(
        std::function<core::Result<T, core::Error>(Connection*)> transaction_fn
    ) {
        auto conn_result = acquire_connection();
        if (!conn_result) {
            return core::Result<T, core::Error>::Err(conn_result.error());
        }

        auto conn = std::move(conn_result.value());

        // Begin transaction
        auto begin_result = conn->begin();
        if (!begin_result) {
            return core::Result<T, core::Error>::Err(begin_result.error());
        }

        // Execute transaction function
        auto txn_result = transaction_fn(conn.get());
        if (!txn_result) {
            // Rollback on error
            conn->rollback();
            return core::Result<T, core::Error>::Err(txn_result.error());
        }

        // Commit on success
        auto commit_result = conn->commit();
        if (!commit_result) {
            return core::Result<T, core::Error>::Err(commit_result.error());
        }

        return txn_result; // Return the result of the transaction function
    }

    class ResultGaurd {
        public:
        explicit ResultGaurd(PGresult* res) : res_(res) {}
        ~ResultGaurd() {
            if (res_ != nullptr) {
                PQclear(res_);
            }
        }

        ResultGaurd(const ResultGaurd&) = delete;
        ResultGaurd& operator=(const ResultGaurd&) = delete;

        PGresult* get() const { return res_; }
        PGresult* release() {
            auto tmp = res_;
            res_ = nullptr;
            return tmp;
        }

        private:
        PGresult* res_;
    };
}
