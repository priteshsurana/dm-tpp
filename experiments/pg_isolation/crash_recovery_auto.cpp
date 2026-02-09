#include <libpq-fe.h>
#include <iostream>
#include <thread>
#include <chrono>
#include <csignal>
#include <atomic>

#ifdef _WIN32
#include <windows.h>
#include <processthreadsapi.h>
#else
#include <unistd.h>
#endif

std::atomic<bool> crash_triggered{false};

// Killer thread: waits then kills process
void killer_thread() {
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    std::cout << "\n[KILLER] Sending termination signal (simulating crash)..." << std::endl;
    crash_triggered = true;
    
#ifdef _WIN32
    TerminateProcess(GetCurrentProcess(), 1);
#else
    kill(getpid(), SIGKILL);
#endif
}

int main(int argc, char* argv[]) {
    const char* conninfo = "host=localhost port=5433 dbname=dm_tpp user=dm_tpp_user password=postgres";
    const char* test_key = "crash-test-auto-001";
    
    // Check if this is the "verify" run
    if (argc > 1 && std::string(argv[1]) == "--verify") {
        std::cout << "=====================================" << std::endl;
        std::cout << " Crash Recovery Verification" << std::endl;
        std::cout << "=====================================" << std::endl;
        
        PGconn* conn = PQconnectdb(conninfo);
        if (PQstatus(conn) != CONNECTION_OK) {
            std::cerr << "Connection failed: " << PQerrorMessage(conn) << std::endl;
            return 1;
        }
        
        const char* query = "SELECT COUNT(*) FROM transactions WHERE idempotency_key = $1";
        const char* params[1] = { test_key };
        PGresult* res = PQexecParams(conn, query, 1, nullptr, params, nullptr, nullptr, 0);
        
        int count = std::atoi(PQgetvalue(res, 0, 0));
        std::cout << "Rows with idempotency_key '" << test_key << "': " << count << std::endl;
        
        if (count == 0) {
            std::cout << "✓ PASS: Transaction was rolled back (no row found)" << std::endl;
            std::cout << "✓ Postgres correctly discarded uncommitted data" << std::endl;
        } else {
            std::cout << "✗ FAIL: Found " << count << " row(s), expected 0" << std::endl;
        }
        
        PQclear(res);
        PQfinish(conn);
        return (count == 0) ? 0 : 1;
    }
    
    // Normal run: attempt transaction and crash mid-way
    std::cout << "=====================================" << std::endl;
    std::cout << " Crash Recovery Experiment" << std::endl;
    std::cout << "=====================================" << std::endl;
    std::cout << "This process will crash in 500ms..." << std::endl;
    std::cout << "After crash, re-run with --verify flag" << std::endl;
    std::cout << "=====================================" << std::endl << std::endl;
    
    PGconn* conn = PQconnectdb(conninfo);
    if (PQstatus(conn) != CONNECTION_OK) {
        std::cerr << "Connection failed: " << PQerrorMessage(conn) << std::endl;
        return 1;
    }
    
    // Cleanup previous test data
    const char* cleanup = "DELETE FROM transactions WHERE idempotency_key = $1";
    const char* params[1] = { test_key };
    PQexecParams(conn, cleanup, 1, nullptr, params, nullptr, nullptr, 0);
    
    // Start killer thread
    std::thread killer(killer_thread);
    killer.detach();
    
    // BEGIN transaction
    std::cout << "[Main] BEGIN transaction" << std::endl;
    PQexec(conn, "BEGIN");
    
    // INSERT data
    std::cout << "[Main] Executing INSERT..." << std::endl;
    const char* insert = "INSERT INTO transactions (idempotency_key, state, payload) "
                         "VALUES ($1, 'RECEIVED', '{\"test\": \"crash_recovery\"}')";
    PGresult* res = PQexecParams(conn, insert, 1, nullptr, params, nullptr, nullptr, 0);
    
    if (PQresultStatus(res) == PGRES_COMMAND_OK) {
        std::cout << "[Main] ✓ INSERT successful (NOT COMMITTED YET)" << std::endl;
    } else {
        std::cerr << "[Main] ✗ INSERT failed: " << PQerrorMessage(conn) << std::endl;
    }
    PQclear(res);
    
    // Delay to let killer thread act
    std::cout << "[Main] Waiting before COMMIT..." << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(1));
    
    // If we reach here, killer failed
    if (!crash_triggered) {
        std::cout << "[Main] COMMIT transaction (this should NOT print)" << std::endl;
        PQexec(conn, "COMMIT");
    }
    
    PQfinish(conn);
    return 0;
}