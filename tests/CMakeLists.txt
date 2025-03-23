enable_testing()

set(AVL_TESTS_SOURCES
    AVLTreeTest.cpp
)

add_executable(avlTest ${AVL_TESTS_SOURCES})
target_include_directories(avlTest PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(avlTest AVLTree GTest::GTest)
add_test(NAME AVLTreeTest COMMAND avlTest)


set(HMAP_TESTS_SOURCES
    HMapTest.cpp
)
add_executable(hmapTest ${HMAP_TESTS_SOURCES})
target_include_directories(hmapTest PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(hmapTest HMap GTest::GTest)
add_test(NAME HMapTest COMMAND hmapTest)



