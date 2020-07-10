cmake_minimum_required(VERSION 3.5)
project(pcsr2)

set(CMAKE_CXX_STANDARD 14)

set(CMAKE_BUILD_TYPE Release)

# add_compile_options (-Wall -g -ggdb)
add_compile_options(-Wall -O3)

file(GLOB_RECURSE HEADERS ${PROJECT_SOURCE_DIR}/*.h)
set(SOURCES main.cpp thread_pool_pppcsr/thread_pool_pppcsr.cpp pcsr/PCSR.cpp
        pppcsr/PPPCSR.cpp thread_pool/thread_pool.cpp)

include_directories(.)
add_executable(pcsr2 ${SOURCES})

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)
target_link_libraries(pcsr2 PRIVATE Threads::Threads numa)

include(${CMAKE_SOURCE_DIR}/cmake/modules/clang-format.cmake)
