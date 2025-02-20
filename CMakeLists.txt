cmake_minimum_required(VERSION 3.13)  # CMake version check
project(ownRedis)               # Create project "simple_example"
set(CMAKE_CXX_STANDARD 11)   # Enable c++14 standard

find_package(GTest REQUIRED)
find_package(Boost REQUIRED COMPONENTS container)

# Если Boost не найден, завершаем с ошибкой
if (NOT Boost_FOUND)
    message(FATAL_ERROR "Boost not found!")
endif ()

set(SERVER_FILES src/server.cpp
        src/Protocol.cpp)
add_executable(server ${SERVER_FILES})
target_link_libraries(server Boost::boost)

set(CLIENT_FILES src/TcpClient.cpp
        src/TcpClient.h
        src/clientClass.cpp
)
add_executable(client src/clientCopied.cpp src/Protocol.cpp)
