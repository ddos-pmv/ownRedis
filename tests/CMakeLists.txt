enable_testing()

set(TESTS_SOURCES
    AVLTreeTest.cpp
)

add_executable(tests ${TESTS_SOURCES})
target_include_directories(tests PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(tests AVLTree GTest::GTest)

add_test(NAME AVLTreeTest COMMAND tests)


