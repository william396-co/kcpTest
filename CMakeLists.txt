cmake_minimum_required(VERSION 3.12)

project(kcpTest LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Threads REQUIRED)

add_library(kcp_core STATIC
    src/ikcp.c
    src/random_util.cpp
    src/udpsocket.cpp
    src/util.cpp
)

target_include_directories(kcp_core
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(kcp_core PUBLIC Threads::Threads)

if(WIN32)
    target_compile_definitions(kcp_core PUBLIC
        WIN32_LEAN_AND_MEAN
        NOMINMAX
        _CRT_SECURE_NO_WARNINGS
        _WINSOCK_DEPRECATED_NO_WARNINGS
    )
    target_link_libraries(kcp_core PUBLIC ws2_32)
endif()

if(MSVC)
    target_compile_options(kcp_core PRIVATE /W4)
else()
    target_compile_options(kcp_core PRIVATE -Wall)
endif()

add_executable(kcp_server
    kcp_server.cpp
    server.cpp
    src/connection.cpp
)
target_link_libraries(kcp_server PRIVATE kcp_core)

add_executable(kcp_client
    kcp_client.cpp
    client.cpp
)
target_link_libraries(kcp_client PRIVATE kcp_core)

add_executable(kcp_multi_client
    kcp_multi_client.cpp
    client.cpp
)
target_link_libraries(kcp_multi_client PRIVATE kcp_core)

add_executable(kcp_test
    test.cpp
)
target_link_libraries(kcp_test PRIVATE kcp_core)
