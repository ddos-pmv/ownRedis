cmake_minimum_required(VERSION 3.13)  # CMake version check
project(ownRedis)               # Create project "simple_example"
set(CMAKE_CXX_STANDARD 11)            # Enable c++14 standard

set(SOURCE_FILES src/server.cpp)

add_executable(ownRedis ${SOURCE_FILES})