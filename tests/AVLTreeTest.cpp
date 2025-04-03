#include <avl.h>
#include <gtest/gtest.h>
#include <utils.h>

#include <cassert>
#include <cstddef>
#include <iostream>
#include <set>

using AVLNode = ownredis::AVLNode;
struct Data {
  AVLNode node;
  uint32_t val = 0;
};

struct Container {
  AVLNode *root = nullptr;
};

static void add(Container &c, uint32_t val) {
  auto *data = new Data();
  avl_init(&data->node);
  data->val = val;

  AVLNode *cur = nullptr;
  AVLNode **from = &c.root;

  while (*from) {
    cur = *from;
    uint32_t node_val = container_of(cur, Data, node)->val;
    from = (val < node_val) ? &cur->left : &cur->right;
  }

  *from = &data->node;
  data->node.parent = cur;
  c.root = avl_fix(&data->node);
}

static bool del(Container &c, uint32_t val) {
  AVLNode *cur = c.root;
  while (cur) {
    uint32_t node_val = container_of(cur, Data, node)->val;
    if (val == node_val) {
      break;
    }

    cur = (val < node_val) ? cur->left : cur->right;
  }
  if (!cur) return false;
  c.root = avl_del(cur);
  delete container_of(cur, Data, node);
  return true;
}

static void avl_verify(AVLNode *parent, AVLNode *node) {
  if (!node) {
    return;
  }
  EXPECT_EQ(parent, node->parent);
  avl_verify(node, node->left);
  avl_verify(node, node->right);

  EXPECT_EQ(node->cnt, 1 + avl_cnt(node->left) + avl_cnt(node->right));

  uint32_t l = avl_height(node->left);
  uint32_t r = avl_height(node->right);
  EXPECT_TRUE(l == r || l == r + 1 || r == l + 1);
  EXPECT_EQ(node->height, std::max(l, r) + 1);

  uint32_t val = container_of(node, Data, node)->val;
  if (node->left) {
    EXPECT_EQ(node->left->parent, node);
    EXPECT_LE(container_of(node->left, Data, node)->val, val);
  }
  if (node->right) {
    EXPECT_EQ(node->right->parent, node);
    EXPECT_GE(container_of(node->right, Data, node)->val, val);
  }
}

static void extract(AVLNode *node, std::multiset<uint32_t> &extracted) {
  if (!node) {
    return;
  }
  extract(node->left, extracted);
  extract(node->right, extracted);
  extracted.insert(container_of(node, Data, node)->val);
}

static void container_verify(Container &c, const std::multiset<uint32_t> &ref) {
  avl_verify(nullptr, c.root);
  EXPECT_EQ(avl_cnt(c.root), ref.size());
  std::multiset<uint32_t> extracted;
  extract(c.root, extracted);
  EXPECT_EQ(extracted, ref);
}

static void dispose(Container &c) {
  while (c.root) {
    AVLNode *node = c.root;
    c.root = avl_del(c.root);
    delete container_of(node, Data, node);
  }
}

TEST(AVLTree, InsertSingleValue) {
  Container c;

  container_verify(c, {});

  add(c, 123);
  container_verify(c, {123});

  EXPECT_FALSE(del(c, 124));
  EXPECT_TRUE(del(c, 123));

  container_verify(c, {});

  dispose(c);
}

TEST(AVLTree, InsertMulitipleValues) {
  Container c;
  std::multiset<uint32_t> ref;

  for (int i = 0; i < 100; i++) {
    add(c, i);
    ref.insert(i);
    container_verify(c, ref);
  }

  dispose(c);
  container_verify(c, {});
}

TEST(AVLTree, InsertDuplecateValues) {
  Container c;
  std::multiset<uint32_t> ref;
  for (int val = 0; val < 20; val++) {
    Container c;
    std::multiset<uint32_t> ref;
    for (int i = 0; i < 100; i++) {
      add(c, i);
      ref.insert(i);
    }

    container_verify(c, ref);
    add(c, val);
    ref.insert(val);
    container_verify(c, ref);
    dispose(c);
  }
}

TEST(AVLTree, RemoveValues) {
  for (int val = 0; val < 200; val++) {
    Container c;
    std::multiset<uint32_t> ref;

    for (int i = 0; i < 200; i++) {
      add(c, i);
      ref.insert(i);
    }

    EXPECT_TRUE(del(c, val));
    ref.erase(val);
    container_verify(c, ref);

    dispose(c);
  }
}

TEST(AVLTree, TestOffsetFromMin) {
  int sz = 200;

  Container c;
  for (int i = 0; i < sz; i++) {
    add(c, i);
  }

  AVLNode *min = c.root;
  while (min->left) {
    min = min->left;
  }

  for (int i = 0; i < sz; i++) {
    AVLNode *node = avl_offset(min, i);
    // ASSERT_NE(node, nullptr);
    ASSERT_EQ(container_of(node, Data, node)->val, i);

    for (int j = 0; j < sz; j++) {
      int64_t offset = j - i;
      AVLNode *node2 = avl_offset(node, offset);
      // ASSERT_NE( node2, null)
      ASSERT_EQ(container_of(node2, Data, node)->val, j);
    }

    ASSERT_EQ(avl_offset(node, sz - i), nullptr);
    ASSERT_EQ(avl_offset(node, -(i + 1)), nullptr);
  }

  dispose(c);
}

int main() {
  ::testing::InitGoogleTest();

  return RUN_ALL_TESTS();
}