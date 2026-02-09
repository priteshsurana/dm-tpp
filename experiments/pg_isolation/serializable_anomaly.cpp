#include <libpq-fe.h>
#include <iostream>
#include <string>


void test_isolation_level(const char* isolation_level, const char* conninfo) {
    std::cout << "\n=== Testing " << isolation_level << " ===" << std::endl;
    
    PGconn* conn_a = PQconnectdb(conninfo);
    PGconn* conn_b = PQconnectdb(conninfo);
    
    if (PQstatus(conn_a) != CONNECTION_OK || PQstatus(conn_b) != CONNECTION_OK) {
        std::cerr << "Connection failed\n";
        return;
    }
    
    // Set isolation level for transaction A
    std::string set_isolation = "SET TRANSACTION ISOLATION LEVEL ";
    set_isolation += isolation_level;
    
    // Transaction A: BEGIN and first SELECT
    PQexec(conn_a, "BEGIN");
    PQexec(conn_a, set_isolation.c_str());
    
    PGresult* res = PQexec(conn_a, "SELECT COUNT(*) FROM transactions WHERE state='RECEIVED'");
    int count_before = std::atoi(PQgetvalue(res, 0, 0));
    std::cout << "Transaction A: First count = " << count_before << std::endl;
    PQclear(res);
    
    // Transaction B: INSERT and COMMIT
    std::cout << "Transaction B: Inserting new row and committing..." << std::endl;
    PQexec(conn_b, "BEGIN");
    const char* insert = "INSERT INTO transactions (idempotency_key, state) "
                         "VALUES (gen_random_uuid()::text, 'RECEIVED')";
    PQexec(conn_b, insert);
    PQexec(conn_b, "COMMIT");
    
    // Transaction A: second SELECT
    res = PQexec(conn_a, "SELECT COUNT(*) FROM transactions WHERE state='RECEIVED'");
    int count_after = std::atoi(PQgetvalue(res, 0, 0));
    std::cout << "Transaction A: Second count = " << count_after << std::endl;
    PQclear(res);
    
    PQexec(conn_a, "COMMIT");
    
    // Analyze result
    if (count_after > count_before) {
        std::cout << "Result: Non-repeatable read (count increased)" << std::endl;
    } else {
        std::cout << "Result: Repeatable read (count stayed same)" << std::endl;
    }
    
    PQfinish(conn_a);
    PQfinish(conn_b);
}

int main() {
    const char* conninfo = "host=localhost port=5433 dbname=dm_tpp user=dm_tpp_user password=postgres";
    
    std::cout << "=== Isolation Level Comparison Experiment ===" << std::endl;
    
    // Clean up
    PGconn* conn = PQconnectdb(conninfo);
    PQexec(conn, "DELETE FROM transactions WHERE state='RECEIVED'");
    PQfinish(conn);
    
    test_isolation_level("READ COMMITTED", conninfo);
    test_isolation_level("REPEATABLE READ", conninfo);
    test_isolation_level("SERIALIZABLE", conninfo);
    
    return 0;
}