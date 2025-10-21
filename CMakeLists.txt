cmake_minimum_required(VERSION 3.15)
project(TC375_Simulator VERSION 1.0.0 LANGUAGES CXX)

# C++ Standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Compiler flags
set(CMAKE_CXX_FLAGS_DEBUG "-g -O0 -Wall -Wextra")
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG")

# Output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

# Find required packages
find_package(OpenSSL REQUIRED)

# Include nlohmann/json (header-only)
find_package(nlohmann_json 3.2.0 QUIET)
if(NOT nlohmann_json_FOUND)
    include(FetchContent)
    FetchContent_Declare(
        nlohmann_json
        URL https://github.com/nlohmann/json/releases/download/v3.11.2/json.tar.xz
    )
    FetchContent_MakeAvailable(nlohmann_json)
endif()

# Include directories
include_directories(
    ${CMAKE_SOURCE_DIR}/tc375_simulator/include
    ${OPENSSL_INCLUDE_DIR}
)

# Source files
set(SIMULATOR_SOURCES
    tc375_simulator/src/tls_client.cpp
    tc375_simulator/src/device_simulator.cpp
    tc375_simulator/src/protocol.cpp
    tc375_simulator/main.cpp
)

# Executable
add_executable(tc375_simulator ${SIMULATOR_SOURCES})

target_link_libraries(tc375_simulator
    ${OPENSSL_LIBRARIES}
    nlohmann_json::nlohmann_json
    pthread
)

# Print configuration
message(STATUS "")
message(STATUS "========================================")
message(STATUS "TC375 Simulator Configuration")
message(STATUS "========================================")
message(STATUS "Build type:        ${CMAKE_BUILD_TYPE}")
message(STATUS "C++ standard:      ${CMAKE_CXX_STANDARD}")
message(STATUS "OpenSSL:           ${OPENSSL_VERSION}")
message(STATUS "========================================")
message(STATUS "")

