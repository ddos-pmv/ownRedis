#include <gtest/gtest.h>

#include <cstddef>

#include "hashtable.h"
#include "utils.h"

using HMap = ownredis::HMap;
using HNode = ownredis::HNode;

static struct {
  HMap db;
} g_data;

struct Entry {
  HNode node;
  std::string key;
  std::string value;
};

static bool entry_eq(HNode *lhs, HNode *rhs) {
  Entry *le = container_of(lhs, Entry, node);
  Entry *re = container_of(rhs, Entry, node);
  return le->key == re->key;
}

TEST(HMapTest, insertAndLookup) {
  HMap map;
  std::unordered_map<std::string, std::string> ref;
  int sz = 300000;

  for (int i = 1000; i < sz; i++) {
    std::string key = std::to_string(i);
    Entry *ent = new Entry{
        .node{.hcode = str_hash((const uint8_t *)key.data(), key.size())},
        .key = key,
        .value = std::to_string(i * 2)};
    if (i == 131) {
      hm_insert(&map, &ent->node);
    } else {
      hm_insert(&map, &ent->node);
    }
  }

  for (int i = 1000; i < sz; i++) {
    Entry key;
    key.key = std::to_string(i);
    key.node.hcode = str_hash((const uint8_t *)key.key.data(), key.key.size());

    HNode *node = hm_lookup(&map, &key.node, entry_eq);

    ASSERT_NE(node, nullptr);
    ASSERT_EQ(container_of(node, Entry, node)->value, std::to_string(i * 2));
    delete container_of(hm_delete(&map, node, entry_eq), Entry, node);
  }
}

TEST(HMapTest, Delete) {
  HMap map;
  std::multiset<Entry *> mset;
  for (int i = 0; i < 1000; i++) {
    std::string tmpKey = std::to_string(i);
    Entry *key =
        new Entry{.key = tmpKey,
                  .value = std::to_string(i * 2),
                  .node{.hcode = str_hash((const uint8_t *)tmpKey.data(),
                                          tmpKey.size())}};
    mset.insert(key);
    hm_insert(&map, &key->node);
  }
  for (int i = 0; i < 1000; i++) {
    std::string tmpKey = std::to_string(i);
    Entry key = Entry{.key = tmpKey,
                      .value = std::to_string(i * 2),
                      .node{.hcode = str_hash((const uint8_t *)tmpKey.data(),
                                              tmpKey.size())}};

    HNode *node = hm_delete(&map, &key.node, entry_eq);
    Entry *ent = container_of(node, Entry, node);

    ASSERT_TRUE(mset.find(ent) != mset.end());
    mset.erase(ent);

    delete ent;
  }

  for (int i = 0; i < 1000; i++) {
    std::string tmpKey = std::to_string(i);
    Entry key = Entry{.key = tmpKey,
                      .value = std::to_string(i * 2),
                      .node{.hcode = str_hash((const uint8_t *)tmpKey.data(),
                                              tmpKey.size())}};

    ASSERT_EQ(hm_delete(&map, &key.node, entry_eq), nullptr);
  }
}

TEST(stdHMapTest, insertAndLookup) {
  int sz = 300000;
  std::unordered_map<std::string, std::string> ref;
  for (int i = 1000; i < sz; i++) {
    ref[std::to_string(i)] = std::to_string(i * 2);
  }
  for (int i = 1000; i < sz; i++) {
    ASSERT_EQ(ref[std::to_string(i)], std::to_string(i * 2));
  }
}

int main() {
  ::testing::InitGoogleTest();
  return RUN_ALL_TESTS();
}