enable_testing()

include_directories(hmapTest PUBLIC ${CMAKE_SOURCE_DIR}/src)

set(AVL_TESTS_SOURCES
    AVLTreeTest.cpp
)

add_executable(avlTest ${AVL_TESTS_SOURCES})
target_link_libraries(avlTest PRIVATE AVLTree GTest::GTest Utils)
add_test(NAME AVLTreeTest COMMAND avlTest)


set(HMAP_TESTS_SOURCES
    HMapTest.cpp
)
add_executable(hmapTest ${HMAP_TESTS_SOURCES})
target_link_libraries(hmapTest PRIVATE HMap GTest::GTest Utils)
add_test(NAME HMapTest COMMAND hmapTest)

set(ZSET_TESTS_SOURCES
    ZSetTest.cpp
)

add_executable(zsetTest ${ZSET_TESTS_SOURCES})
target_link_libraries(zsetTest PRIVATE GTest::GTest Utils ZSet)
add_test(NAME ZSetTest COMMAND zsetTest)


