add_library(ownredis
 server.cpp
)

target_include_directories(ownredis PUBLIC ${CMAKE_SOURCE_DIR}/include)

target_link_libraries(ownredis Utils Proto ZSet HMap Boost::boost )