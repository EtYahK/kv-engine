cmake_minimum_required(VERSION 3.15)
project(minikv CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
    add_compile_options(/W4 /permissive-)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# --- main server ---
add_executable(minikv
    src/main.cpp
    src/server/tcp_server.cpp
    src/storage/storage.cpp
    src/concurrency/thread_pool.cpp
    src/parser/parser.cpp
    src/utils/aof.cpp
)
target_include_directories(minikv PRIVATE src)

# --- benchmark client ---
add_executable(minikv_bench
    benchmarks/bench.cpp
)
target_include_directories(minikv_bench PRIVATE src)

# threading
find_package(Threads REQUIRED)
target_link_libraries(minikv PRIVATE Threads::Threads)
target_link_libraries(minikv_bench PRIVATE Threads::Threads)

# winsock on Windows
if(WIN32)
    target_link_libraries(minikv PRIVATE ws2_32)
    target_link_libraries(minikv_bench PRIVATE ws2_32)
endif()
