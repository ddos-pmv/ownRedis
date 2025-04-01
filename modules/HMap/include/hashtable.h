#ifndef HASHTABLE_H
#define HASHTABLE_H

#pragma once

#include <unistd.h>

struct HNode {
  uint64_t hcode = 0;
  HNode *next = nullptr;
};

struct HTab {
  HNode **tab = nullptr;
  size_t mask = 0;
  size_t size = 0;
};

struct HMap {
  HTab newer;
  HTab older;
  size_t migrate_pos = 0;
};

HNode *hm_lookup(HMap *map, HNode *hkey, bool (*eq)(HNode *, HNode *));

void hm_insert(HMap *map, HNode *key);

HNode *hm_delete(HMap *map, HNode *key, bool (*eq)(HNode *, HNode *));

void hm_clear(HMap *map);

size_t hm_size(HMap *map);

void hm_foreach(HMap *map, bool (*f)(HNode *, void *), void *arg);

#endif  // HASHTABLE_H
