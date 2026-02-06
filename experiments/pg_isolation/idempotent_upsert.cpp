#include <libpq-fe.h>
#include <iostream>

// Experiment: INSERT ... ON CONFLICT semantics

// Scenario:
// - INSERT with idempotency key
// - INSERT again with same key
// - Verify: second insert is n0-op, returns nothing

int main() {
    const char* conninfo = "host=localhost port=5432 dbname=dm_tpp user=dm_tpp_user password=postgres";
    const char* test_key = "upsert-test-001";

    std::cout <<"**********Idempotent upsert experiment**********" << std::endl;

    PGconn* conn = PQconnectdb(conninfo);
    if (PQstatus(conn) != CONNECTION_OK) {
        std::cerr << "Connection to database failed: " << PQerrorMessage(conn) << std::endl;
        return 1;   
    }

    std::cout << "Connected to database." << std::endl;
    const char* cleanup = "DELETE FROM transactions WHERE idempotency_key = $1";
    const char* params[1] = { test_key };
    PGresult* res = PQexecParams(conn, cleanup, 1, nullptr, params, nullptr, nullptr, 0);
    PQclear(res);

    const char* insert_sql = 
    "INSERT INTO transactions (idempotency_key, state, payload) "
    "VALUES ($1, 'RECEIVED', '{\"amount\":100}') "
    "ON CONFLICT (idempotency_key) DO NOTHING "
    "RETURNING id, state";

    std::cout << "Performing first insert..." << std::endl;
    res = PQexecParams(conn, insert_sql, 1, nullptr, params, nullptr, nullptr, 0);

    if (PQresultStatus(res) == PGRES_TUPLES_OK) {
        int ntuples = PQntuples(res);
        if (ntuples > 0) {
            std::cout << "ID: " << PQgetvalue(res, 0, 0) 
                      << ", State: " << PQgetvalue(res, 0, 1) << std::endl;
        
        }
    }
    PQclear(res);

    std::cout << "Performing second insert (should be no-op)..." << std::endl;
    res = PQexecParams(conn, insert_sql, 1, nullptr, params, nullptr, nullptr, 0);

    bool test2_passed = false;
    if (PQresultStatus(res) == PGRES_TUPLES_OK) {
        int ntuples = PQntuples(res);
        if (ntuples == 0) {
            std::cout << "Second insert correctly resulted in no rows affected." << std::endl;
            test2_passed = true;
        } else {
            std::cout << "Unexpected rows returned on second insert!" << std::endl;
        }
    } else {
        std::cout << "Error on second insert: " << PQerrorMessage(conn) << std::endl;
    }
    PQclear(res);


    // Verify final state
    std:: cout << "Verify final state ..." << std::endl;
    const char* count_sql = "SELECT COUNT(*) FROM transactions WHERE idempotency_key = $1";
    res = PQexecParams(conn, count_sql, 1, nullptr, params, nullptr, nullptr, 0);

    bool test3_passed = false;
    if (PQresultStatus(res) == PGRES_TUPLES_OK) {
        int count = atoi(PQgetvalue(res, 0, 0));
        if (count == 1) {
            std::cout << "Final state verification passed: one record exists." << std::endl;
            test3_passed = true;
        } else {
            std::cout << "Final state verification failed: expected 1 record, found " << count << std::endl;
        }
    } else {
        std::cout << "Error during final state verification: " << PQerrorMessage(conn) << std::endl;
    }
    PQclear(res);
    PQfinish(conn);

    if (test2_passed && test3_passed) {
        std::cout << "Idempotent upsert experiment PASSED." << std::endl;
        return 0;
    } else {
        std::cout << "Idempotent upsert experiment FAILED." << std::endl;
        return 1;
    }
}