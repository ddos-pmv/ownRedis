#ifndef UTILS_H
#define UTILS_H

#include <unistd.h>

#include <cstdint>

#define container_of(ptr, type, member)                \
  ({                                                   \
    const typeof(((type *)0)->member) *__mptr = (ptr); \
    (type *)((char *)__mptr - offsetof(type, member)); \
  })
// FNV hash
uint64_t str_hash(const uint8_t *data, size_t len) {
  uint32_t h = 0x811c9dc5;
  for (int i = 0; i < len; i++) {
    h = (h + data[i]) * 0x01000193;
  }
  return h;
}

#endif