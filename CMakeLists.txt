cmake_minimum_required(VERSION 3.20)
project(dm-tpp VERSION 1.0.0 LANGUAGES CXX)

# C++17 standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Build options
option(BUILD_TESTS "Build unit tests" ON)

# Global include directories
include_directories(${PROJECT_SOURCE_DIR}/lib)


# Dependencies
find_package(nlohmann_json 3.10.0 REQUIRED)

# Tests (if enabled)
if(BUILD_TESTS)
    enable_testing()
    find_package(GTest REQUIRED)
endif()

# Add subdirectories
add_subdirectory(lib/core)