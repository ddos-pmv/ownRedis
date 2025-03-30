#include "hashtable.h"

#include <cassert>

static void h_init(HTab *htab, size_t n) {
  assert(n > 0 && ((n & (n - 1)) == 0));  // n - power of 2
  htab->tab = (HNode **)calloc(n, sizeof(HNode *));
  htab->mask = n - 1;
  htab->size = 0;
}

static void h_insert(HTab *htab, HNode *hnode) {
  size_t pos = hnode->hcode & htab->mask;
  HNode *next = htab->tab[pos];
  hnode->next = next;
  htab->tab[pos] = hnode;
  htab->size++;
}

static HNode **h_lookup(HTab *htab, HNode *key, bool (*eq)(HNode *, HNode *)) {
  if (!htab->tab) return nullptr;

  size_t pos = key->hcode & htab->mask;
  HNode **from = &htab->tab[pos];
  for (HNode *cur; (cur = *from) != nullptr; from = &cur->next) {
    bool firstCond = cur->hcode == key->hcode;
    if (cur->hcode == key->hcode && eq(cur, key)) {
      return from;
    }
  }

  return nullptr;
}

static HNode *h_detach(HTab *htab, HNode **from) {
  HNode *cur = *from;  // not to delete
  *from = cur->next;
  htab->size--;
  return cur;
}

const size_t k_reheshing_work = 128;

static void hm_help_rehashing(HMap *hmap) {
  size_t nwork = 0;
  while (nwork < k_reheshing_work && hmap->older.size > 0) {
    HNode **from = &hmap->older.tab[hmap->migrate_pos];
    if (!*from) {
      hmap->migrate_pos++;
      continue;
    }

    h_insert(&hmap->newer, h_detach(&hmap->older, from));
    nwork++;
  }

  if (hmap->older.size == 0 && hmap->older.tab) {
    free(hmap->older.tab);
    hmap->older.tab = nullptr;
    hmap->older = HTab{};
  }
}

static void hm_trigger_rehashing(HMap *hmap) {
  assert(hmap->older.tab == nullptr);

  hmap->older = hmap->newer;
  h_init(&hmap->newer, (hmap->newer.mask + 1) * 2);
  hmap->migrate_pos = 0;
}

HNode *hm_lookup(HMap *hmap, HNode *key, bool (*eq)(HNode *, HNode *)) {
  hm_help_rehashing(hmap);

  HNode **from = h_lookup(&hmap->newer, key, eq);

  if (!from) {
    from = h_lookup(&hmap->older, key, eq);
  }

  return from ? *from : nullptr;
}

const size_t k_max_load_factor = 8;

void hm_insert(HMap *hmap, HNode *key) {
  if (!hmap->newer.tab) {
    h_init(&hmap->newer, 4);
  }

  h_insert(&hmap->newer, key);

  if (!hmap->older.tab) {
    // loadFactor = hmap->newer.size / (hmap->newer.mask+1) >= k_max_load_factor

    size_t shreshold = (hmap->newer.mask + 1) * k_max_load_factor;
    if (hmap->newer.size >= shreshold) {
      hm_trigger_rehashing(hmap);
    }
  }
  hm_help_rehashing(hmap);
}

HNode *hm_delete(HMap *hmap, HNode *key, bool (*eq)(HNode *, HNode *)) {
  hm_help_rehashing(hmap);
  if (HNode **from = h_lookup(&hmap->newer, key, eq)) {
    return h_detach(&hmap->newer, from);
  }

  if (HNode **from = h_lookup(&hmap->older, key, eq)) {
    return h_detach(&hmap->older, from);
  }
  return nullptr;
}

void hm_clear(HMap *hmap) {
  free(hmap->older.tab);
  free(hmap->newer.tab);
  *hmap = HMap{};
}

size_t hm_size(HMap *hmap) { return hmap->older.size + hmap->newer.size; }

bool h_foreach(HTab *htab, bool (*f)(HNode *, void *), void *out) {
  for (int i = 0; htab->mask != 0 && i <= htab->mask; i++) {
    for (HNode *node = htab->tab[i]; node != nullptr; node = node->next) {
      if (!f(node, out)) return false;
    }
  }
  return true;
}

void hm_foreach(HMap *map, bool (*f)(HNode *, void *), void *arg) {
  h_foreach(&map->newer, f, arg) && h_foreach(&map->older, f, arg);
}
