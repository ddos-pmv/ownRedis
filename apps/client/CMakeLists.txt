file(GLOB SOURCES "src/*.cpp")
file(GLOB HEADERS "include/*.h")

add_executable(client ${HEADERS} ${SOURCES} )

target_link_libraries(client PUBLIC Utils Proto Boost::boost)