#pragma once
// h-files
#include "types.h"
// own libs
#include <Proto/buf_utils.h>
#include <utils.h>
// system
#include <arpa/inet.h>
#include <netinet/ip.h>
#include <sys/socket.h>
#include <unistd.h>
// C/C++
#include <cassert>
#include <iostream>
#include <string>
#include <vector>

namespace {
bool read_u32(const uint8_t *&src, const uint8_t *srcEnd, uint32_t *dst) {
  if (src + 4 > srcEnd) {
    msg("read_u32() error");
    return false;
  }

  std::memcpy(dst, src, 4);
  src += 4;
  return true;
}

bool read_str(const uint8_t *&src, const uint8_t *srcEnd, uint32_t strLen,
              std::string &dist) {
  if (src + strLen > srcEnd) {
    msg("read_str() error");
    return false;
  }

  dist.assign(src, src + strLen);
  src += strLen;
  return true;
}
}  // namespace

namespace ownredis {
namespace proto {

inline int32_t write_all(int fd, const char *buf, size_t n) {
  while (n > 0) {
    ssize_t rv = write(fd, buf, n);
    if (rv <= 0) {
      return -1;  // error
    }
    assert((size_t)rv <= n);
    n -= (size_t)rv;
    buf += rv;
  }
  return 0;
}
inline int32_t read_full(int fd, char *buf, size_t n) {
  while (n > 0) {
    ssize_t rv = read(fd, buf, n);
    if (rv <= 0) {
      return -1;  // error, or unexpected EOF
    }
    assert((size_t)rv <= n);
    n -= (size_t)rv;
    buf += rv;
  }
  return 0;
}

inline int32_t parse_response(const uint8_t *src,
                              std::vector<std::string> &dist, size_t srcLen) {
  if (srcLen < 1) {
    msg("bad response");
    return -1;
  }

  switch (src[0]) {
    case ownredis::TAG_NIL:
      dist.push_back("NILL");
      return 1;
    //     1     |     4     |       4       | err_msg_len
    // [TAG_ERR] | [ERRCODE] | [ERR_MSG_LEN] |  [ERR_MSG]
    case ownredis::TAG_ERR:
      if (srcLen < 1 + 8) {
        msg("bad response");
        return -1;
      }
      {
        int32_t error_code = 0;
        int32_t error_msg_len = 0;
        std::memcpy(&error_code, &src[1], 4);
        std::memcpy(&error_msg_len, &src[1 + 4], 4);
        if (srcLen < 1 + 8 + error_msg_len) {
          msg("bad response");
          return -1;
        }

        dist.push_back("ERR");
        dist.push_back(std::to_string(error_code));
        dist.push_back(std::string(reinterpret_cast<const char *>(&src[1 + 8]),
                                   error_msg_len));

        return 1 + 8 + error_msg_len;
      }
    //     1     |     4     |   STR_LEN   |
    // [TAG_STR] | [STR_LEN] |    [STR]    |
    case ownredis::TAG_STR:
      if (srcLen < 1 + 4) {
        msg("bad response");
        return -1;
      }
      {
        uint32_t str_len = 0;
        std::memcpy(&str_len, &src[1], 4);
        if (srcLen < 1 + 4 + str_len) {
          msg("bad response");
          return -1;
        }
        dist.push_back("STR");
        dist.push_back(std::to_string(str_len));
        dist.push_back(
            std::string(reinterpret_cast<const char *>(&src[1 + 4]), str_len));
        return 1 + 4 + str_len;
      }
    //     1     |       8      |
    // [TAG_INT] |   [INT_VAL]  |
    case ownredis::TAG_INT:
      if (srcLen < 1 + 8) {
        msg("bad response");
        return -1;
      }
      {
        int64_t int_val = 0;
        std::memcpy(&int_val, &src[1], 8);
        dist.push_back("INT");
        dist.push_back(std::to_string(int_val));
        return 1 + 8;
      }
    //     1     |        8        |
    // [TAG_DBL] |   [DOUBLE_VAL]  |
    case ownredis::TAG_DBL:
      if (srcLen < 1 + 8) {
        msg("bad respponse");
        return -1;
      }
      {
        double dbl_val = 0;
        std::memcpy(&dbl_val, &src[1], 8);
        dist.push_back("DBL");
        dist.push_back(std::to_string(dbl_val));
        return 1 + 8;
      }

    //     1     |       4      |       1        |    first_el_bytes
    // [TAG_ARR] |   [ARR_LEN]  | [FIRST_EL_TAG] |
    case ownredis::TAG_ARR:
      if (srcLen < 1 + 4) {
        msg("bad response");
        return false;
      }
      {
        uint32_t arr_len = 0;
        std::memcmp(&arr_len, &src[1], 4);
        dist.push_back("ARR_BEGIN");
        dist.push_back(std::to_string(arr_len));
        size_t arr_bytes = 1 + 4;
        for (uint32_t i = 0; i < arr_len; i++) {
          int32_t rv =
              parse_response(&src[arr_bytes], dist, arr_len - arr_bytes);
          if (rv < 0) {
            return rv;
          }
          arr_bytes += static_cast<size_t>(rv);
        }
        dist.push_back("ARR_END");
        return arr_bytes;
      }
    default:
      msg("bad response");
      return -1;
  }
}

inline int32_t parse_request(const uint8_t *src, std::vector<std::string> &dist,
                             size_t srcLen) {
  const uint8_t *srcEnd = src + srcLen;
  uint32_t nstr = 0;
  if (!read_u32(src, srcEnd, &nstr)) {
    msg("Failed to read nstr");
    return -1;
  }

  if (nstr > k_max_args) {
    msg("Too many args");
    return -1;  //! safety limit
  }

  dist.reserve(nstr);
  while (dist.size() < nstr) {
    uint32_t len = 0;
    if (!read_u32(src, srcEnd, &len)) {
      return -1;
    }
    dist.emplace_back();
    if (!read_str(src, srcEnd, len, dist.back())) {
      return -1;
    }
  }
  if (src != srcEnd) {
    msg("src != srcEnd");
    return -1;
  }

  return 0;
}
}  // namespace proto

}  // namespace ownredis