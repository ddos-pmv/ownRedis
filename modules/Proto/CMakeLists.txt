find_package(Boost REQUIRED)

add_library(Proto INTERFACE
        include/Proto/types.h
        include/Proto/out_utils.h
        include/Proto/out_utils.h)

target_link_libraries(Proto INTERFACE Boost::boost)

# Указываем путь к заголовкам
target_include_directories(Proto INTERFACE 
 ${CMAKE_CURRENT_SOURCE_DIR}/include
)
