cmake_minimum_required(VERSION 3.10)
project(PQC_Tools)

# C Standard
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Include directories
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/..)

# PQC Simulator
add_executable(pqc_simulator
    pqc_simulator.c
    ../common/protocol/pqc_params.c
)

# Install
install(TARGETS pqc_simulator DESTINATION bin)

# Build type
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_options(pqc_simulator PRIVATE -g -O0 -Wall -Wextra)
else()
    target_compile_options(pqc_simulator PRIVATE -O2 -Wall)
endif()

