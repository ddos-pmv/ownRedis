cmake_minimum_required(VERSION 3.13)  # CMake version check
project(ownRedis)               # Create project "simple_example"
set(CMAKE_CXX_STANDARD 11)   # Enable c++14 standard

find_package(GTest REQUIRED)


#set(SOURCE_FILES src/server.cpp)
#add_executable(ownRedis ${SOURCE_FILES})


#set(TEST_FILES tests/main.cpp src/server.cpp)
#add_executable(ownRedisTest ${TEST_FILES})
#target_link_libraries(ownRedisTest GTest::GTest)

find_package(benchmark REQUIRED)
set(BENCHMARK_FILES
        #        benchmark/BM_Buf_append.cpp
        benchmark/BM_Server.cpp)
add_executable(ownRedisBenchmark ${BENCHMARK_FILES})
target_link_libraries(ownRedisBenchmark benchmark::benchmark)