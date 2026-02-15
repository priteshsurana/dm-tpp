#include "postgres_client.h"
#include <chrono>
#include <thread>
#include <sstream>

namespace dmtpp::persistence {
    using namespace core;

    Connection::Connection(PGconn* conn, ConnectionPool* pool) : conn_(conn), pool_(pool), in_transaction_(false) {}
    Connection::~Connection() {
        // Rollback any uncommitted transaction
        if (in_transaction_ && conn_) {
            PGresult* res = PQexec(conn_, "ROLLBACK");
            if (res) PQclear(res);
        }

        // Return connection to pool
        if(pool_ && conn_) {
            static_cast<PostgresClient*>(pool_)->release_connection(conn_);
        }

    }

    Connection::Connection(Connection&& other) noexcept : conn_(other.conn_), pool_(other.pool_), in_transaction_(other.in_transaction_) {
        other.conn_ = nullptr;
        other.pool_ = nullptr;
    }

    Connection& Connection::operator=(Connection&& other) noexcept {
        if (this != &other) {
            // Clean up current connection
            if (in_transaction_ && conn_) {
                PGresult* res = PQexec(conn_, "ROLLBACK");
                if (res) PQclear(res);
            }
            if(pool_ && conn_) {
                static_cast<PostgresClient*>(pool_)->release_connection(conn_);
            }

            // Move from other
            conn_ = other.conn_;
            pool_ = other.pool_;
            in_transaction_ = other.in_transaction_;

            // Null out other
            other.conn_ = nullptr;
            other.pool_ = nullptr;
        }
        return *this;
    }

    Result<PGresult*, Error> Connection::execute(const std::string& query) {
        if (!conn_) {
            return Result<PGresult*, Error>::err(Error::transient("postgres_execute", "Connection is null")
                .with_code(E_TRANSIENT_POSTGRES_CONNECTION));
        }

        PGresult* res = PQexec(conn_, query.c_str());

        if (!res) {
            return Result<PGresult*, Error>::err(pg_error_to_error("execute"));
        }

        ExecStatusType status = PQresultStatus(res);
        if (status != PGRES_COMMAND_OK && status != PGRES_TUPLES_OK) {
            std::string error_msg = PQresultErrorMessage(res);
            PQclear(res);
            
            //Check for deadlock (transient)
            if (error_msg.find("deadlock detected") != std::string::npos) {
                return Result<PGresult*, Error>::err(Error(E_TRANSIENT_POSTGRES_DEADLOCK, "postgres_execute", "Deadlock detected", error_msg));
            }

            // Check for serialization failure (transient)
            if (error_msg.find("could not serialize") != std::string::npos) {
                return Result<PGresult*, Error>::err(Error(E_TRANSIENT_POSTGRES_DEADLOCK, "postgres_execute", "Serialization failure", error_msg));
            }

            //Other sql errors are permanent (bad query, constraint violation, etc.)
            return Result<PGresult*, Error>::err(Error(E_PERMANENT_INVALID_INPUT, "postgres_execute", "SQL error", error_msg));
        }

        return Result<PGresult*, Error>::ok(res);
    }

    Result<PGresult*, Error> Connection::execute_params(const std::string& query, const std::vector<std::string>& params) {
        if (!conn_) {
            return Result<PGresult*, Error>::err(Error::transient("postgres_execute_params", "Connection is null")
                .with_code(E_TRANSIENT_POSTGRES_CONNECTION));
        }

        // Convert params to array of C strings
        std::vector<const char*> param_values;
        for (const auto& param : params) {
            param_values.push_back(param.c_str());
        }

        PGresult* res = PQexecParams(conn_, query.c_str(), static_cast<int>(params.size()), nullptr, param_values.data(), nullptr, nullptr, 0);

        if (!res) {
            return Result<PGresult*, Error>::err(pg_error_to_error("execute_params"));
        }

        ExecStatusType status = PQresultStatus(res);
        if (status != PGRES_COMMAND_OK && status != PGRES_TUPLES_OK) {
            std::string error_msg = PQresultErrorMessage(res);
            PQclear(res);
            
            //Check for deadlock (transient)
            if (error_msg.find("deadlock detected") != std::string::npos) {
                return Result<PGresult*, Error>::err(Error(E_TRANSIENT_POSTGRES_DEADLOCK, "postgres_execute_params", "Deadlock detected", error_msg));
            }

            // Check for serialization failure (transient)
            if (error_msg.find("could not serialize") != std::string::npos) {
                return Result<PGresult*, Error>::err(Error(E_TRANSIENT_POSTGRES_DEADLOCK, "postgres_execute_params", "Serialization failure", error_msg));
            }

            //Other sql errors are permanent (bad query, constraint violation, etc.)
            return Result<PGresult*, Error>::err(Error(E_PERMANENT_INVALID_INPUT, "postgres_execute_params", "SQL error", error_msg));

        }

        return Result<PGresult*, Error>::ok(res);
    }
    
    Result<void, Error> Connection::begin() {
        auto res_result = execute("BEGIN");
        if (!res_result) {
            return Result<void, Error>::err(res_result.error());
        }

        PQclear(res_result.unwrap());
        in_transaction_ = true;
        return Result<void, Error>::ok();
    }

    Result<void, Error> Connection::commit() {
        auto res_result = execute("COMMIT");
        if (!res_result) {
            return Result<void, Error>::err(res_result.error());
        }

        PQclear(res_result.unwrap());
        in_transaction_ = false;
        return Result<void, Error>::ok();
    }

    Result<void, Error> Connection::rollback() {
        auto res_result = execute("ROLLBACK");
        if (!res_result) {
            return Result<void, Error>::err(res_result.error());
        }

        PQclear(res_result.unwrap());
        in_transaction_ = false;
        return Result<void, Error>::ok();
    }

    bool Connection::is_healthy() const {
        if (!conn_) return false;
        return PQstatus(conn_) == CONNECTION_OK;
    }

    Error Connection::pg_error_to_error(const std::string& operation) const {
        std::string msg = conn_ ? PQerrorMessage(conn_) : "Connection is null";
        
        // Check if it's a connection error (transient)
        if (msg.find("server closed the connection") != std::string::npos ||
            msg.find("could not connect") != std::string::npos ||
            msg.find("connection refused") != std::string::npos ||
            msg.find("timeout") != std::string::npos) {
            return Error::transient(operation, msg)
                .with_code(E_TRANSIENT_POSTGRES_CONNECTION);
        }
        
        // Other errors are likely query/data issues (permanent)
        return Error::permanent(operation, msg)
            .with_code(E_PERMANENT_INVALID_INPUT);
    }


    // PostgresClient implementation
    PostgresClient::PostgresClient(const std::string& connection_string) : connection_string_(connection_string) {}
    PostgresClient::~PostgresClient() {
        // Clean up all connections in the pool
        std::lock_guard<std::mutex> lock(mutex_);
        for (auto& conn : pool_) {
            if (conn) {
                PQfinish(conn);
            }
        }
    }

    Result<void, Error> PostgresClient::initialize(size_t pool_size) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        if (initialized_) {
            return Result<void, Error>::err(
                Error::permanent("postgres_init", "Already initialized")
                    .with_code(E_PERMANENT_INVARIANT_VIOLATED)
            );
        }
        
        pool_.reserve(pool_size);
        in_use_.reserve(pool_size);
        
        for (size_t i = 0; i < pool_size; ++i) {
            auto conn_result = create_connection();
            if (!conn_result) {
                // Cleanup partial initialization
                for (auto conn : pool_) {
                    if (conn) PQfinish(conn);
                }
                pool_.clear();
                in_use_.clear();
                return Result<void, Error>::err(conn_result.error());
            }
            
            pool_.push_back(conn_result.value());
            in_use_.push_back(false);
        }
        
        initialized_ = true;
        return Result<void, Error>::ok();
    }

    Result<std::unique_ptr<Connection>, Error> PostgresClient::acquire_connection(
        std::chrono::milliseconds timeout
    ) {
        std::unique_lock<std::mutex> lock(mutex_);
        
        if (!initialized_) {
            return Result<std::unique_ptr<Connection>, Error>::err(
                Error::permanent("acquire_connection", "Pool not initialized")
                    .with_code(E_PERMANENT_INVARIANT_VIOLATED)
            );
        }
        
        auto deadline = std::chrono::steady_clock::now() + timeout;
        
        while (true) {
            // Find available connection
            for (size_t i = 0; i < pool_.size(); ++i) {
                if (!in_use_[i]) {
                    // Check health
                    if (PQstatus(pool_[i]) != CONNECTION_OK) {
                        // Try to reconnect
                        PQfinish(pool_[i]);
                        auto new_conn = create_connection();
                        if (!new_conn) continue;  // Skip unhealthy connection
                        pool_[i] = new_conn.value();
                    }
                    
                    in_use_[i] = true;
                    return Result<std::unique_ptr<Connection>, Error>::ok(
                        std::make_unique<Connection>(pool_[i], this)
                    );
                }
            }
            
            // No available connections - wait for one to be released
            if (cv_.wait_until(lock, deadline) == std::cv_status::timeout) {
                return Result<std::unique_ptr<Connection>, Error>::err(
                    Error::transient("acquire_connection", "Timeout waiting for connection")
                        .with_code(E_TRANSIENT_RESOURCE_EXHAUSTED)
                );
            }
        }
    }

    Result<PGresult*, Error> PostgresClient::execute(const std::string& query) {
        auto conn_result = acquire_connection();
        if (!conn_result) return Result<PGresult*, Error>::err(conn_result.error());
        
        return conn_result.value()->execute(query);
    }

    Result<PGresult*, Error> PostgresClient::execute_params(
        const std::string& query,
        const std::vector<std::string>& params
    ) {
        auto conn_result = acquire_connection();
        if (!conn_result) return Result<PGresult*, Error>::err(conn_result.error());
        
        return conn_result.value()->execute_params(query, params);
    }

    bool PostgresClient::is_healthy() const {
        std::lock_guard<std::mutex> lock(mutex_);
        
        if (!initialized_ || pool_.empty()) return false;
        
        // At least one connection must be healthy
        for (auto conn : pool_) {
            if (conn && PQstatus(conn) == CONNECTION_OK) {
                return true;
            }
        }
        
        return false;
    }

    size_t PostgresClient::pool_size() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return pool_.size();
    }

    size_t PostgresClient::available_connections() const {
        std::lock_guard<std::mutex> lock(mutex_);
        size_t count = 0;
        for (bool used : in_use_) {
            if (!used) ++count;
        }
        return count;
    }

    void PostgresClient::release_connection(PGconn* conn) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        for (size_t i = 0; i < pool_.size(); ++i) {
            if (pool_[i] == conn) {
                in_use_[i] = false;
                cv_.notify_one();  // Wake up one waiting thread
                return;
            }
        }
    }

    Result<PGconn*, Error> PostgresClient::create_connection() {
        PGconn* conn = PQconnectdb(connection_string_.c_str());
        
        if (!conn) {
            return Result<PGconn*, Error>::err(
                Error::transient("create_connection", "PQconnectdb returned null")
                    .with_code(E_TRANSIENT_POSTGRES_CONNECTION)
            );
        }
        
        if (PQstatus(conn) != CONNECTION_OK) {
            std::string error_msg = PQerrorMessage(conn);
            PQfinish(conn);
            return Result<PGconn*, Error>::err(
                Error::transient("create_connection", error_msg)
                    .with_code(E_TRANSIENT_POSTGRES_CONNECTION)
            );
        }
        
        return Result<PGconn*, Error>::ok(conn);
    }
}

    