#pragma once

#include "buf_utils.h"
#include <string>

namespace ownredis {
namespace proto {
inline void out_nil(Buffer &buf) { buf_append_u8(buf, TAG_NIL); }

inline void out_str(Buffer &out, const char *s, size_t len) {
  buf_append_u8(out, TAG_STR);

  buf_append_u32(out, static_cast<uint32_t>(len));
  buf_append(out, reinterpret_cast<const uint8_t *>(s), len);
}

inline void out_int(Buffer &out, int64_t val) {
  buf_append_u8(out, TAG_INT);
  buf_append_i64(out, val);
}

inline void out_arr(Buffer &out, uint32_t n) {
  buf_append_u8(out, TAG_ARR);
  buf_append_u32(out, n);
}

inline size_t out_begin_arr(Buffer &out) {
  buf_append_u8(out, TAG_ARR);
  buf_append_u32(out, 0);
  return out.size() - 4;
}

inline void out_end_arr(Buffer &out, size_t ctx, uint32_t n) {
  assert(out[ctx - 1] == TAG_ARR);
  std::memcpy(&out[ctx], &n, 4);
}

inline void out_dbl(Buffer &out, double val) {
  buf_append_u8(out, TAG_DBL);
  buf_append_dbl(out, val);
}

inline void out_err(Buffer &out, uint32_t errCode, const std::string &msg) {
  buf_append_u8(out, TAG_ERR);
  buf_append_u32(out, errCode);
  buf_append_u32(out, static_cast<uint32_t>(msg.size()));
  buf_append(out, reinterpret_cast<const uint8_t *>(msg.data()), msg.size());
}

}  // namespace proto
}  // namespace ownredis
