file(GLOB SOURCES "src/*.cpp")

add_library(ZSet STATIC ${SOURCES})

target_link_libraries(ZSet PUBLIC Utils HMap AVLTree)
target_include_directories(ZSet PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
