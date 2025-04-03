cmake_minimum_required(VERSION 3.13)  # CMake version check
project(ownRedis)               # Create project "simple_example"
set(CMAKE_CXX_STANDARD 20)
find_package(GTest REQUIRED)
find_package(Boost REQUIRED COMPONENTS container)

if (NOT GTest_FOUND)
        message(FATAL_ERROR "GTest not found!")
endif()

# Boost не найден, завершаем с ошибкой
if (NOT Boost_FOUND)
    message(FATAL_ERROR "Boost not found!")
endif ()



add_subdirectory(modules/AVLTree)
add_subdirectory(modules/HMap)
add_subdirectory(modules/ZSet)
add_subdirectory(modules/Utils)
add_subdirectory(modules/Proto)

add_subdirectory(apps/server)
add_subdirectory(apps/client)

add_subdirectory(tests)

message(STATUS "Project" ${PROJECT_NAME} "configured successfully")

