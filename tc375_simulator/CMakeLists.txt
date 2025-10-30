cmake_minimum_required(VERSION 3.15)
project(tc375_simulator VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# OpenSSL 3.x required for PQC (optional, for PQC client)
find_package(OpenSSL 3.0 QUIET)
if(OpenSSL_FOUND AND OPENSSL_VERSION VERSION_LESS "3.0")
    message(WARNING "OpenSSL 3.0+ recommended for PQC support")
endif()

# mbedTLS for DoIP (required)
find_package(MbedTLS REQUIRED)

# Find threads
find_package(Threads REQUIRED)

# Include directories
include_directories(
    ${CMAKE_SOURCE_DIR}/include
    ${OPENSSL_INCLUDE_DIR}
)

# TC375 Simulator (original)
add_executable(tc375_simulator
    main.cpp
    src/device_simulator.cpp
    src/device_info.cpp
    src/ota_manager.cpp
    src/protocol.cpp
    src/uds_handler.cpp
    src/tls_client.cpp
)

# TC375 PQC Client (optional, for PQC-enabled external servers)
if(OpenSSL_FOUND)
    add_executable(tc375_pqc_client
        main_pqc_client.cpp
        src/pqc_doip_client.cpp
    )
    target_link_libraries(tc375_pqc_client
        OpenSSL::SSL
        OpenSSL::Crypto
        ${CMAKE_THREAD_LIBS_INIT}
    )
endif()

# TC375 mbedTLS Client (for DoIP with VMG)
add_executable(tc375_doip_client
    src/doip_client_mbedtls.cpp
)

# Link libraries
target_link_libraries(tc375_simulator
    OpenSSL::SSL
    OpenSSL::Crypto
    ${CMAKE_THREAD_LIBS_INIT}
)

target_link_libraries(tc375_doip_client
    MbedTLS::mbedtls
    MbedTLS::mbedx509
    MbedTLS::mbedcrypto
    ${CMAKE_THREAD_LIBS_INIT}
)

# Compiler flags
target_compile_options(tc375_simulator PRIVATE
    -Wall -Wextra
    $<$<CONFIG:Release>:-O3>
    $<$<CONFIG:Debug>:-g -O0>
)

target_compile_options(tc375_pqc_client PRIVATE
    -Wall -Wextra
    $<$<CONFIG:Release>:-O3>
    $<$<CONFIG:Debug>:-g -O0>
)

# Install
install(TARGETS tc375_simulator tc375_pqc_client
    RUNTIME DESTINATION bin
)

