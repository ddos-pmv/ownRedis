file(GLOB SOURCES "src/*.cpp")
file(GLOB HEADERS "include/*.h")

add_library(HMap STATIC ${SOURCES})

# Указываем где искать свои заголовки
target_include_directories(HMap PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# Подключаем зависимости
target_link_libraries(HMap PRIVATE Utils)