add_library(Utils INTERFACE)

# Указываем путь к заголовкам
target_include_directories(Utils INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)
