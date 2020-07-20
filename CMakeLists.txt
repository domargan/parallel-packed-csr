cmake_minimum_required(VERSION 3.8 FATAL_ERROR)#
set(CMAKE_VERBOSE_MAKEFILE ON)

project(parallel-packed-csr VERSION 0.1 LANGUAGES CXX)

include(${CMAKE_SOURCE_DIR}/cmake/modules/clang-format.cmake)

if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif ()

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_FLAGS "-std=c++14 -ansi -pedantic-errors -Wall -Wextra -pthread -DUPDATES_NO_TASKS -DRECOMPUTE_ONLY_BREAKAGES_DEL")
set(CMAKE_CXX_FLAGS_DEBUG "-g -O0 -DPRINT_TEST")
set(CMAKE_CXX_FLAGS_RELEASE "-O3")

#set(Boost_INCLUDE_DIR /opt/boost/include)
#set(Boost_LIBRARY_DIR /opt/boost/lib)
#find_package(Boost COMPONENTS system filesystem iostreams REQUIRED)
#include_directories(${Boost_INCLUDE_DIR})
#link_directories(${Boost_LIBRARY_DIR})

set(PROJECT_SOURCE_DIR ${CMAKE_SOURCE_DIR}/src)

file(GLOB_RECURSE parallel-packed-csr_SOURCES "${PROJECT_SOURCE_DIR}/*.cpp")
file(GLOB_RECURSE parallel-packed-csr_HEADERS "${PROJECT_SOURCE_DIR}/*.h")

set(parallel-packed-csr_INCLUDE_DIRS "")
foreach (_headerFile ${parallel-packed-csr_HEADERS})
    get_filename_component(_dir ${_headerFile} PATH)
    list(APPEND parallel-packed-csr_INCLUDE_DIRS ${_dir})
endforeach ()
list(REMOVE_DUPLICATES parallel-packed-csr_INCLUDE_DIRS)

add_executable(parallel-packed-csr ${parallel-packed-csr_SOURCES})
target_include_directories(parallel-packed-csr PRIVATE ${parallel-packed-csr_INCLUDE_DIRS})
#target_link_libraries(parallel-packed-csr ${Boost_LIBRARIES})

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)
target_link_libraries(parallel-packed-csr PRIVATE Threads::Threads numa)

#list(REMOVE_ITEM parallel-packed-csr_SOURCES ${PROJECT_SOURCE_DIR}/main.cpp)