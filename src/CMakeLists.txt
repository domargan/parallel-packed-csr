cmake_minimum_required(VERSION 3.5)
project(pcsr2)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_FLAGS -pthread)

add_executable(pcsr2 main.cpp)