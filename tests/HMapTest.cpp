#include <gtest/gtest.h>

#include <cstddef>

#include "hashtable.h"

#define container_of(ptr, type, member)                \
  ({                                                   \
    const typeof(((type *)0)->member) *__mptr = (ptr); \
    (type *)((char *)__mptr - offsetof(type, member)); \
  })

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

// FNV hash
static uint64_t str_hash(const uint8_t *data, size_t len) {
  uint32_t h = 0x811c9dc5;
  for (int i = 0; i < len; i++) {
    h = (h + data[i]) * 0x01000193;
  }
  return h;
}

TEST(HMapTest, insertAndLookup) {
  HMap map;
  std::unordered_map<std::string, std::string> ref;
  for (int i = 1000; i < 30000; i++) {
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

  for (int i = 1000; i < 30000; i++) {
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
  std::unordered_map<std::string, std::string> ref;
  for (int i = 1000; i < 30000; i++) {
    ref[std::to_string(i)] = std::to_string(i * 2);
  }
  for (int i = 1000; i < 30000; i++) {
    ASSERT_EQ(ref[std::to_string(i)], std::to_string(i * 2));
  }
}

int main() {
  ::testing::InitGoogleTest();
  return RUN_ALL_TESTS();
}