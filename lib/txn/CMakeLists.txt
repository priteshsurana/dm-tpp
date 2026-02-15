#Transaction library
add_library(txn STATIC
    src/idempotency.cpp
)

target_include_directories(txn
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/lib
)

target_link_libraries(txn
    PUBLIC 
        core
        persistence
)