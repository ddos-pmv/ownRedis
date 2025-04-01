#include <gtest/gtest.h>
#include <zset.h>

struct Data {
  std::string str;
  ZSet* zset;
};

struct Container {
  ZSet zset;
};

TEST(ZSet, testInsertionAndDeletetion) {
  int sz = 200;
  Container c;
  for (int i = 0; i < sz; i++) {
    std::string strToInsert = std::to_string(i);
    ASSERT_TRUE(
        zset_insert(&c.zset, strToInsert.c_str(), strToInsert.size(), i));
  }

  for (int i = 0; i < sz; i++) {
    std::string s = std::to_string(i);
    ZNode* node = zset_lookup(&c.zset, s.c_str(), s.size());
    ASSERT_EQ(node->score, i);
    std::string s2(node->name, node->len);
    ASSERT_EQ(s, s2);
    zset_delete(&c.zset, node);
    ASSERT_EQ(nullptr, zset_lookup(&c.zset, s.c_str(), s.size()));
  }

  zset_clear(&c.zset);
}

TEST(ZSet, TestSeekGe) {
  Container c;

  ASSERT_TRUE(zset_insert(&c.zset, "abc", 3, 1));
  ASSERT_TRUE(zset_insert(&c.zset, "bcd", 3, 1));
  ASSERT_TRUE(zset_insert(&c.zset, "a", 1, 0));
  ASSERT_TRUE(zset_insert(&c.zset, "c", 1, 2));

  ZNode* node0 = zset_lookup(&c.zset, "a", 1);
  ZNode* node1 = zset_lookup(&c.zset, "abc", 3);
  ZNode* node2 = zset_lookup(&c.zset, "bcd", 3);

  ASSERT_EQ(node0, zset_seekge(&c.zset, 0, "a", 1));
  ASSERT_EQ(node1, zset_seekge(&c.zset, 1, "a", 1));
  ASSERT_EQ(node2, zset_seekge(&c.zset, 1, "b", 1));

  zset_clear(&c.zset);
}




int main() {
  ::testing::InitGoogleTest();

  return RUN_ALL_TESTS();
}