#include "zset.h"

#include <utils.h>

#include <iostream>

namespace ownredis {

static ZNode* znode_new(const char* name, size_t len, double score) {
  ZNode* node = new (len) ZNode;
  assert(node);
  avl_init(&node->tree);
  node->hmap.next = nullptr;
  node->hmap.hcode = str_hash(reinterpret_cast<const uint8_t*>(name), len);
  node->score = score;
  node->len = len;
  std::memcpy(&node->name[0], name, len);
  return node;
}

static void znode_del(ZNode* node) { delete node; }

static size_t min(size_t lhs, size_t rhs) { return lhs < rhs ? lhs : rhs; }

// compare by score name tuple

// compare AVLNode and ZNode
static bool zless(AVLNode* lhs, double score, const char* name, size_t len) {
  ZNode* zl = container_of(lhs, ZNode, tree);
  if (zl->score != score) {
    return zl->score < score;
  }
  int rv = std::memcmp(zl->name, name, min(zl->len, len));
  if (rv != 0) {
    return rv < 0;
  }
  return zl->len < len;
}
static bool zless(AVLNode* lhs, AVLNode* rhs) {
  ZNode* node = container_of(rhs, ZNode, tree);
  return zless(lhs, node->score, node->name, node->len);
}

static void tree_insert(ZSet* set, ZNode* node) {
  AVLNode* parent = nullptr;
  AVLNode** from = &set->root;

  while (*from) {
    parent = *from;
    from = zless(&node->tree, parent) ? &parent->left : &parent->right;
  }

  *from = &node->tree;
  node->tree.parent = parent;
  set->root = avl_fix(&node->tree);
}

// update the score of on existing node
static void zset_update(ZSet* zset, ZNode* node, double score) {
  if (node->score == score) return;

  // detache tree node
  zset->root = avl_del(&node->tree);
  avl_init(&node->tree);

  // reinsert tree node
  node->score = score;
  tree_insert(zset, node);
}

bool zset_insert(ZSet* zset, const char* name, size_t len, double score) {
  ZNode* node = zset_lookup(zset, name, len);
  if (node) {
    zset_update(zset, node, score);
    return false;
  }
  node = znode_new(name, len, score);
  hm_insert(&zset->hmap, &node->hmap);
  tree_insert(zset, node);
  return true;
}

struct HKey {
  HNode node;
  const char* name = nullptr;
  size_t len;
};

static bool hcmp(HNode* node, HNode* key) {
  ZNode* znode = container_of(node, ZNode, hmap);
  HKey* hkey = container_of(key, HKey, node);

  if (znode->len != hkey->len) {
    return false;
  }

  return 0 == std::memcmp(znode->name, hkey->name, znode->len);
}

ZNode* zset_lookup(ZSet* zset, const char* name, size_t len) {
  if (!zset->root) {
    return nullptr;
  }

  HKey key;
  key.node.hcode = str_hash(reinterpret_cast<const uint8_t*>(name), len);
  key.name = name;
  key.len = len;
  HNode* found = hm_lookup(&zset->hmap, &key.node, hcmp);

  return found ? container_of(found, ZNode, hmap) : nullptr;
}

void zset_delete(ZSet* zset, ZNode* node) {
  HKey key;
  key.node.hcode = node->hmap.hcode;
  key.name = node->name;
  key.len = node->len;

  HNode* found = hm_delete(&zset->hmap, &key.node, hcmp);
  assert(found);

  zset->root = avl_del(&node->tree);
  delete node;
}

ZNode* zset_seekge(ZSet* zset, double score, const char* name, size_t len) {
  AVLNode* found = nullptr;

  for (AVLNode* node = zset->root; node;) {
    if (zless(node, score, name, len)) {
      node = node->right;
    } else {
      found = node;
      node = node->left;
    }
  }

  return found ? container_of(found, ZNode, tree) : nullptr;
}

ZNode* zset_offset(ZNode* node, int64_t offset) {
  AVLNode* tnode = node ? avl_offset(&node->tree, offset) : nullptr;
  return tnode ? container_of(tnode, ZNode, tree) : nullptr;
}

void tree_dispose(AVLNode* node) {
  if (!node) return;
  tree_dispose(node->left);
  tree_dispose(node->right);

  delete container_of(node, ZNode, tree);
}

void zset_clear(ZSet* zset) {
  hm_clear(&zset->hmap);
  tree_dispose(zset->root);

  zset->root = nullptr;
}

}  // namespace ownredis