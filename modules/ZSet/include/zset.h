#pragma once

#include <avl.h>
#include <hashtable.h>

namespace ownredis {

struct ZSet {
  AVLNode* root = nullptr;
  HMap hmap;
};

struct ZNode {
  AVLNode tree;
  HNode hmap;
  double score;
  size_t len;
  char name[0];

  static void* operator new(size_t structSize, size_t name_len = 0) {
    return malloc(structSize + name_len);
  }
  static void operator delete(void* ptr) { free(ptr); }
};

bool zset_insert(ZSet* set, const char* name, size_t len, double score);
ZNode* zset_lookup(ZSet* set, const char* name, size_t len);
void zset_delete(ZSet* set, ZNode* node);
ZNode* zset_seekge(ZSet* set, double score, const char* name, size_t len);
ZNode* zset_offset(ZNode* node, int64_t offset);
void zset_clear(ZSet* set);

}  // namespace ownredis
