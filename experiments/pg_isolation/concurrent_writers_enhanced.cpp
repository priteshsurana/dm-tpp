#include <libpq-fe.h>
#include <iostream>
#include <vector>
#include <thread>
#include <chrono>
#include <atomic>
#include <random>
#include <string>

constexpr int NUM_THREADS = 20;
constexpr const char* IDEMPOTENCY_KEY = "concurrent-writer-001";

std::atomic<int> success_count{0};
std::atomic<int> failure_count{0};
std::atomic<int> constraint_violations{0};

// Random delay
int random_delay_ms() {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> dis(10, 100);
    return dis(gen);
}

void worker_with_delay(int thread_id, const char* conninfo) {
    // Random start delay to spread out attempts
    std::this_thread::sleep_for(std::chrono::milliseconds(random_delay_ms()));
    PGconn* conn = PQconnectdb(conninfo);
    if (PQstatus(conn) != CONNECTION_OK) {
        std::cerr << "Thread " << thread_id << ": Connection failed: " << PQerrorMessage(conn) << std::endl;
        //failure_count++;
        PQfinish(conn);
        return;
    }

    // Begin transaction
    PQexec(conn, "BEGIN");

    PQexec(conn, "SELECT pg_sleep(0.05)"); // Simulate some processing delay

    // Attempt insert
    const char* insert_sql = 
        "INSERT INTO transactions (idempotency_key, state, payload) "
        "VALUES ($1, 'RECEIVED', $2)";
    std::string payload = "{\"thread_id\":" + std::to_string(thread_id) + "}";
    const char* params[2] = { IDEMPOTENCY_KEY, payload.c_str() };

    PGresult* res = PQexecParams(conn, insert_sql, 2, nullptr, params, nullptr, nullptr, 0);
    ExecStatusType status = PQresultStatus(res);

    if (status == PGRES_COMMAND_OK) {
        std::cout << "[" << thread_id << "] Insert succeeded." << std::endl;
        success_count++;
        PQexec(conn, "COMMIT");
    } else {
        std::string error_msg = PQerrorMessage(conn);
        if (error_msg.find("unique constraint") != std::string::npos ||
            error_msg.find("duplicate key") != std::string::npos) {
            std::cout << "[" << thread_id << "] Insert failed due to constraint violation." << std::endl;
            constraint_violations++;
        } else {
            std::cout << "[" << thread_id << "] Insert failed with unexpected error: " << error_msg << std::endl;
        }
        failure_count++;
        PQexec(conn, "ROLLBACK");
    }
    PQclear(res);
    PQfinish(conn);
}


int main() {
    const char* conninfo = "host=localhost port=5432 dbname=dm_tpp user=dm_tpp_user password=postgres";
    std::cout <<" Concurrent Writers Enhanced Experiment" << std::endl;
    std::cout <<" Threads: " << NUM_THREADS << std::endl;
    std::cout <<" Idempotency Key: " << IDEMPOTENCY_KEY << std::endl;
    std::cout <<" Random delays" << std::endl;

    //Cleanup
    PGconn* conn = PQconnectdb(conninfo);
    const char* cleanup_sql = "DELETE FROM transactions WHERE idempotency_key = $1";
    const char* cleanup_params[1] = { IDEMPOTENCY_KEY };
    PQexecParams(conn, cleanup_sql, 1, nullptr, cleanup_params, nullptr, nullptr, 0);
    PQfinish(conn);

    // Launch threads
    auto start_time = std::chrono::high_resolution_clock::now();
    std::vector<std::thread> threads;
    for (int i = 0; i < NUM_THREADS; ++i) {
        threads.emplace_back(worker_with_delay, i, conninfo);
    }

    // Join threads
    for (auto& t : threads) {
        t.join();
    }
    auto end_time = std::chrono::steady_clock::now();
    auto duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();

    // Verifing final state
    conn = PQconnectdb(conninfo);
    const char* count_sql = "SELECT COUNT(*) FROM transactions WHERE idempotency_key = $1";
    PGresult* res = PQexecParams(conn, count_sql, 1, nullptr, cleanup_params, nullptr, nullptr, 0);
    int final_count = std::atoi(PQgetvalue(res, 0, 0));
    PQclear(res);
    PQfinish(conn);

    // Results
    std::cout << "********** Experiment Results **********" << std::endl;
    std::cout << "Duration: " << duration_ms << "ms" << std::endl;
    std::cout << "Successful INSERTs: " << success_count << std::endl;
    std::cout << "Failed INSERTs: " << failure_count << std::endl;
    std::cout << "Unique constraint violations: " << constraint_violations << std::endl;
    std::cout << "Final DB row count: " << final_count << std::endl;

        if (final_count == 1 && success_count == 1 && constraint_violations == NUM_THREADS - 1) {
        std::cout << "✓ PASS: Unique constraint is atomic" << std::endl;
        std::cout << "✓ Exactly 1 thread succeeded" << std::endl;
        std::cout << "✓ " << constraint_violations << " constraint violations (as expected)" << std::endl;
        return 0;
    } else {
        std::cout << "✗ FAIL: Unexpected behavior" << std::endl;
        return 1;
    }

}