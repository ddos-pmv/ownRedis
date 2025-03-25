file(GLOB SOURCES "src/*.cpp")
file(GLOB HEADERS "include/*.h")

add_library(AVLTree STATIC ${SOURCES})

target_link_libraries(AVLTree PRIVATE Utils)
target_include_directories(AVLTree PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)