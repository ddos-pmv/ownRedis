add_library(ownredis
 server.cpp
 cli.cpp
)

target_include_directories(ownredis PUBLIC ${CMAKE_SOURCE_DIR}/include)

target_link_libraries(ownredis PRIVATE Utils Proto ZSet HMap Boost::boost )