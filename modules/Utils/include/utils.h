#pragma once

#include <unistd.h>

#include <cstdint>
#include <iostream>

// namespace ownredis {

#define container_of(ptr, type, member)                \
  ({                                                   \
    const typeof(((type *)0)->member) *__mptr = (ptr); \
    (type *)((char *)__mptr - offsetof(type, member)); \
  })

// FNV hash
inline uint64_t str_hash(const uint8_t *data, size_t len) {
  uint32_t h = 0x811c9dc5;
  for (int i = 0; i < len; i++) {
    h = (h + data[i]) * 0x01000193;
  }
  return h;
}

static void die(const char *msg) {
  std::cerr << msg << ": " << std::strerror(errno) << '\n';
  abort();
}

static void msg_errno(const char *msg) {
  std::cerr << "errno: " << std::strerror(errno) << ". " << msg << '\n';
}

static void msg(const char *msg) { std::cout << msg << '\n'; }

// }  // namespace ownredis