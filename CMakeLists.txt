cmake_minimum_required(VERSION 3.8 FATAL_ERROR)#
set(CMAKE_VERBOSE_MAKEFILE ON)

project(parallel-packed-csr VERSION 0.1 LANGUAGES CXX)

if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif ()

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_FLAGS "-std=c++14 -ansi -pedantic-errors -Wall -Wextra -pthread")
set(CMAKE_CXX_FLAGS_DEBUG "-g -O0")
set(CMAKE_CXX_FLAGS_RELEASE "-O3")

set(PROJECT_SOURCE_DIR ${CMAKE_SOURCE_DIR}/src)

file(GLOB_RECURSE parallel-packed-csr_SOURCES "${PROJECT_SOURCE_DIR}/*.cpp")
file(GLOB_RECURSE parallel-packed-csr_HEADERS "${PROJECT_SOURCE_DIR}/*.h")

set(PROJECT_TEST_DIR ${CMAKE_SOURCE_DIR}/test)

file(GLOB_RECURSE parallel-packed-csr_TEST_SOURCES "${PROJECT_TEST_DIR}/*.cpp")
file(GLOB_RECURSE parallel-packed-csr_TEST_HEADERS "${PROJECT_TEST_DIR}/*.h")

include(${CMAKE_SOURCE_DIR}/cmake/modules/clang-format.cmake)

set(parallel-packed-csr_INCLUDE_DIRS "")
foreach (_headerFile ${parallel-packed-csr_HEADERS})
    get_filename_component(_dir ${_headerFile} PATH)
    list(APPEND parallel-packed-csr_INCLUDE_DIRS ${_dir})
endforeach ()
list(REMOVE_DUPLICATES parallel-packed-csr_INCLUDE_DIRS)

add_executable(parallel-packed-csr ${parallel-packed-csr_SOURCES})
target_include_directories(parallel-packed-csr PRIVATE ${parallel-packed-csr_INCLUDE_DIRS})

#set(Boost_INCLUDE_DIR /opt/boost/include)
#set(Boost_LIBRARY_DIR /opt/boost/lib)
#find_package(Boost COMPONENTS system filesystem iostreams REQUIRED)
#include_directories(${Boost_INCLUDE_DIR})
#link_directories(${Boost_LIBRARY_DIR})
#target_link_libraries(parallel-packed-csr ${Boost_LIBRARIES})

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)
target_link_libraries(parallel-packed-csr PRIVATE Threads::Threads numa)

list(REMOVE_ITEM parallel-packed-csr_SOURCES ${PROJECT_SOURCE_DIR}/main.cpp)
add_executable(tests ${parallel-packed-csr_SOURCES} ${parallel-packed-csr_TEST_SOURCES})

find_package(GTest)
if (GTEST_FOUND)
    include_directories(${GTEST_INCLUDE_DIRS})
    target_link_libraries(tests ${GTEST_LIBRARIES} pthread numa)
else()
    include(${CMAKE_SOURCE_DIR}/cmake/CPM.cmake)
    CPMAddPackage(
            NAME googletest
            GITHUB_REPOSITORY google/googletest
            GIT_TAG release-1.8.1
            VERSION 1.8.1
            OPTIONS
            "INSTALL_GTEST OFF"
            "gtest_force_shared_crt ON"
    )
    target_link_libraries(tests gtest gtest_main gmock pthread numa)
endif()

target_include_directories(tests PRIVATE ${parallel-packed-csr_INCLUDE_DIRS} ${PROJECT_TEST_DIR})

include (CTest)
gtest_discover_tests(tests)
