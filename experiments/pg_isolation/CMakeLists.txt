cmake_minimum_required(VERSION 3.17)
project(pg_isolation_experiments)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find PostgreSQL library
find_package(PostgreSQL REQUIRED)

#Experiment1: Idempotent upserts
add_executable(idempotent_upsert idempotent_upsert.cpp)
target_link_libraries(idempotent_upsert PostgreSQL::PostgreSQL)

# Experiment 2: Concurrent writers (enhanced with delays)
add_executable(concurrent_writers_enhanced concurrent_writers_enhanced.cpp)
target_link_libraries(concurrent_writers_enhanced PostgreSQL::PostgreSQL)

# Experiment 3: Crash Recovery (automated)
add_executable(crash_recovery_auto crash_recovery_auto.cpp)
target_link_libraries(crash_recovery_auto PostgreSQL::PostgreSQL)

# Experiment 4: Isolation level comparison
add_executable(serializable_anomaly serializable_anomaly.cpp)
target_link_libraries(serializable_anomaly PostgreSQL::PostgreSQL)

#Experiment 5: Advisory locks
add_executable(advisory_locks advisory_locks.cpp)
target_link_libraries(advisory_locks PostgreSQL::PostgreSQL)