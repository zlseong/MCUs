cmake_minimum_required(VERSION 3.15)
project(vehicle_gateway VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# OpenSSL 3.x required for PQC (external server communication)
find_package(OpenSSL 3.0 REQUIRED)
if(OPENSSL_VERSION VERSION_LESS "3.0")
    message(FATAL_ERROR "OpenSSL 3.0 or higher required for PQC support")
endif()

# mbedTLS for DoIP (TC375 communication)
find_package(MbedTLS REQUIRED)

# Required packages
find_package(Threads REQUIRED)

# Include directories
include_directories(
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/common
    ${OPENSSL_INCLUDE_DIR}
)

# Common library (PQC utilities + mbedTLS DoIP)
add_library(vmg_common STATIC
    common/pqc_config.c
    common/metrics.c
    common/json_output.c
    common/mbedtls_doip.c
)

target_link_libraries(vmg_common
    ${OPENSSL_LIBRARIES}
    MbedTLS::mbedtls
    MbedTLS::mbedx509
    MbedTLS::mbedcrypto
)

# VMG DoIP Server (with mbedTLS - standard TLS 1.3, no PQC)
# This is the PRIMARY server for in-vehicle communication
add_executable(vmg_doip_server
    example_vmg_doip_server_mbedtls.cpp
)

# Plain DoIP Server (legacy, no TLS)
add_executable(vmg_doip_server_plain
    src/doip_server.cpp
    example_vmg_doip_server.cpp
    src/uds_service_handler.cpp
)

# VMG Gateway Main (optional, for testing)
add_executable(vmg_gateway
    src/vmg_gateway.cpp
    src/pqc_tls_server.c
)

target_link_libraries(vmg_doip_server
    vmg_common
    ${CMAKE_THREAD_LIBS_INIT}
)

target_link_libraries(vmg_doip_server_plain
    ${CMAKE_THREAD_LIBS_INIT}
)

target_link_libraries(vmg_gateway
    vmg_common
    ${OPENSSL_LIBRARIES}
    ${CMAKE_THREAD_LIBS_INIT}
)

# VMG HTTPS Client (with PQC)
add_executable(vmg_https_client
    src/https_client.cpp
    src/pqc_tls_client.c
)

target_link_libraries(vmg_https_client
    vmg_common
    ${OPENSSL_LIBRARIES}
    ${CMAKE_THREAD_LIBS_INIT}
)

# VMG MQTT Client (with PQC)
add_executable(vmg_mqtt_client
    src/mqtt_client.cpp
    src/pqc_mqtt.c
)

target_link_libraries(vmg_mqtt_client
    vmg_common
    ${OPENSSL_LIBRARIES}
    ${CMAKE_THREAD_LIBS_INIT}
)

# Compiler flags
target_compile_options(vmg_common PRIVATE
    -Wall -Wextra -Wpedantic
    $<$<CONFIG:Release>:-O3>
    $<$<CONFIG:Debug>:-g -O0>
)

# Install
install(TARGETS vmg_doip_server vmg_doip_server_plain vmg_https_client vmg_mqtt_client
    RUNTIME DESTINATION bin
)

