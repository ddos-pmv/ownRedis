#pragma once

#include "types.h"
namespace ownredis {
namespace proto {

inline void buf_append(Buffer &buf, const uint8_t *data, size_t len) {
  buf.insert(buf.end(), data, data + len);
}

inline void buf_append_u8(Buffer &buf, const uint8_t data) {
  buf.push_back(data);
}

inline void buf_append_u32(Buffer &buf, const uint32_t data) {
  buf.insert(buf.end(), reinterpret_cast<const uint8_t *>(&data),
             reinterpret_cast<const uint8_t *>(&data) + 4);
}

inline void buf_append_i64(Buffer &buf, const int64_t data) {
  buf.insert(buf.end(), reinterpret_cast<const uint8_t *>(&data),
             reinterpret_cast<const uint8_t *>(&data) + 8);
}

inline void buf_append_dbl(Buffer &buf, double data) {
  buf_append(buf, (const uint8_t *)&data, 8);
}

inline void buf_consume(Buffer &buf, size_t n) {
  buf.erase(buf.begin(), buf.begin() + n);
}
}  // namespace proto
}  // namespace ownredis