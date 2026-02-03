# Core library CMake configuration
add_library(core STATIC src/error.cpp)

target_include_directories(core
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/lib
)

target_link_libraries(
    core PUBLIC nlohmann_json::nlohmann_json
)

# Tests
if(BUILD_TESTS)
    add_executable(result_test
        test/result_test.cpp
    )
    target_link_libraries(result_test PRIVATE core GTest::gtest GTest::gtest_main)
    add_test(NAME result_test COMMAND result_test)
endif()

