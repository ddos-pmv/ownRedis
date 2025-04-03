#pragma once

#include <stdint.h>

#include <cassert>

namespace ownredis {

struct AVLNode {
  AVLNode* left = nullptr;
  AVLNode* right = nullptr;
  AVLNode* parent = nullptr;
  uint32_t height = 0;
  uint32_t cnt = 0;  //!< subtree size
};

inline void avl_init(AVLNode* node) {
  node->left = node->right = node->parent = nullptr;
  node->cnt = 1;
  node->height = 1;
}

inline uint32_t avl_height(AVLNode* node) { return node ? node->height : 0; }
inline uint32_t avl_cnt(AVLNode* node) { return node ? node->cnt : 0; }

AVLNode* avl_fix(AVLNode*);
AVLNode* avl_del(AVLNode*);
AVLNode* avl_offset(AVLNode*, int64_t);

}  // namespace ownredis