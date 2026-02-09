#include <libpq-fe.h>
#include <iostream>
#include <thread>
#include <chrono>

void test_row_locks(const char* conninfo) {
    std::cout << "\n=== Row Lock Test ===" << std::endl;
    
    // Setup: ensure row exists
    PGconn* setup_conn = PQconnectdb(conninfo);
    PQexec(setup_conn, "INSERT INTO transactions (id, idempotency_key, state) "
                       "VALUES (1, 'row-lock-test', 'RECEIVED') "
                       "ON CONFLICT (id) DO NOTHING");
    PQfinish(setup_conn);
    
    auto worker_a = [conninfo]() {
        PGconn* conn = PQconnectdb(conninfo);
        std::cout << "Worker A: Acquiring row lock..." << std::endl;
        PQexec(conn, "BEGIN");
        PQexec(conn, "SELECT * FROM transactions WHERE id=1 FOR UPDATE");
        std::cout << "Worker A: Lock acquired, holding for 3 seconds..." << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(3));
        PQexec(conn, "COMMIT");
        std::cout << "Worker A: Released lock" << std::endl;
        PQfinish(conn);
    };
    
    auto worker_b = [conninfo]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        PGconn* conn = PQconnectdb(conninfo);
        std::cout << "Worker B: Attempting to acquire row lock..." << std::endl;
        auto start = std::chrono::steady_clock::now();
        PQexec(conn, "BEGIN");
        PQexec(conn, "SELECT * FROM transactions WHERE id=1 FOR UPDATE");
        auto end = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
        std::cout << "Worker B: Lock acquired after " << duration << "ms" << std::endl;
        PQexec(conn, "COMMIT");
        PQfinish(conn);
    };
    
    std::thread t1(worker_a);
    std::thread t2(worker_b);
    t1.join();
    t2.join();
}

void test_advisory_locks(const char* conninfo) {
    std::cout << "\n=== Advisory Lock Test ===" << std::endl;
    
    auto worker_a = [conninfo]() {
        PGconn* conn = PQconnectdb(conninfo);
        std::cout << "Worker A: Acquiring advisory lock 12345..." << std::endl;
        PQexec(conn, "SELECT pg_advisory_lock(12345)");
        std::cout << "Worker A: Lock acquired, holding for 3 seconds..." << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(3));
        PQexec(conn, "SELECT pg_advisory_unlock(12345)");
        std::cout << "Worker A: Released lock" << std::endl;
        PQfinish(conn);
    };
    
    auto worker_b = [conninfo]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        PGconn* conn = PQconnectdb(conninfo);
        std::cout << "Worker B: Attempting to acquire advisory lock 12345..." << std::endl;
        auto start = std::chrono::steady_clock::now();
        PQexec(conn, "SELECT pg_advisory_lock(12345)");
        auto end = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
        std::cout << "Worker B: Lock acquired after " << duration << "ms" << std::endl;
        PQexec(conn, "SELECT pg_advisory_unlock(12345)");
        PQfinish(conn);
    };
    
    std::thread t1(worker_a);
    std::thread t2(worker_b);
    t1.join();
    t2.join();
}

int main() {
    const char* conninfo = "host=localhost port=5433 dbname=dm_tpp user=dm_tpp_user password=postgres";
    
    std::cout << "=== Lock Comparison Experiment ===" << std::endl;
    
    test_row_locks(conninfo);
    test_advisory_locks(conninfo);
    
    std::cout << "\n=== Conclusions ===" << std::endl;
    std::cout << "- Row locks: Tied to specific rows, released on COMMIT/ROLLBACK" << std::endl;
    std::cout << "- Advisory locks: Application-managed, must be explicitly released" << std::endl;
    std::cout << "- Recommendation: Use row locks for data access" << std::endl;
    
    return 0;
}