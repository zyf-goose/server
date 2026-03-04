cmake_minimum_required(VERSION 3.16)

project(server_reactor LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS ON)

add_library(reactor_core STATIC
    src/acceptor.cpp
    src/connection.cpp
    src/connection_table.cpp
    src/event_loop.cpp
    src/kvcache/cache.cpp
    src/kvcache/kv_handler.cpp
    src/reactor.cpp
    src/sys_utils.cpp
)

target_include_directories(reactor_core
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

add_executable(reactor
    reactor.cpp
)
target_link_libraries(reactor PRIVATE reactor_core)

add_executable(tcp_client
    tcp_client.cpp
)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(reactor_core PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(reactor PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(tcp_client PRIVATE -Wall -Wextra -Wpedantic)
endif()
