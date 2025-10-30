cmake_minimum_required(VERSION 3.10)
project(ZonalGatewayLinux)

# C++ Standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Include directories
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../common
    ${CMAKE_CURRENT_SOURCE_DIR}/../../common/protocol
)

# Source files
set(SOURCES
    src/main.cpp
    src/zonal_gateway_linux.cpp
)

# Executable
add_executable(zonal_gateway_linux ${SOURCES})

# Link libraries
target_link_libraries(zonal_gateway_linux pthread)

# Install
install(TARGETS zonal_gateway_linux DESTINATION bin)

# Build options
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_options(zonal_gateway_linux PRIVATE -g -O0 -Wall -Wextra)
else()
    target_compile_options(zonal_gateway_linux PRIVATE -O2 -Wall)
endif()

