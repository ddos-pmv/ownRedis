file(GLOB SOURCES "src/*.cpp")
file(GLOB HEADERS "include/*.h")

# Создаём исполняемый файл
add_executable(server ${SOURCES})

# Указываем директории для включения заголовков
target_include_directories(server PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        # Добавьте другие пути к заголовкам, если нужно
)

target_link_libraries(server PUBLIC Utils HMap Boost::boost)