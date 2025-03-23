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

enable_testing()

set(SERVER_SOURCES
        src/server.cpp
        src/Protocol.cpp
        
)

set(CLIENT_SOURCES
        src/client.cpp
)

set(HASHTABLE_SOURCES
        src/hashtable.cpp
        src/hashtable.h
)

set(AVLTREE_SOURCES
        src/avl.h
        src/avl.cpp
)

# Сборка сервера
add_executable(server ${SERVER_SOURCES} ${HASHTABLE_SOURCES})
target_link_libraries(server PRIVATE Boost::boost)

# Сборка клиента
add_executable(client ${CLIENT_SOURCES}  ${HASHTABLE_SOURCES})

# Cборка AVL дерева
add_library(AVLTree src/avl.h src/avl.cpp)

# Сборка HMap
add_library(HMap ${HASHTABLE_SOURCES})

add_subdirectory(tests)

message(STATUS "Project" ${PROJECT_NAME} "configured successfully")

