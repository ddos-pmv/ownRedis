#pragma once
#include <boost/container/devector.hpp>

namespace ownredis {
using Buffer = boost::container::devector<uint8_t>;
enum {
  TAG_NIL = 0,  // nil
  TAG_ERR = 1,  // error code + msg
  TAG_STR = 2,  // string
  TAG_INT = 3,  // int64
  TAG_DBL = 4,  // double
  TAG_ARR = 5,  // array
};

enum { ERR_UNKNOWN = 0, ERR_TOO_BIG = 1, ERR_BAD_TYPE = 2, ERR_BAD_ARG = 3 };

constexpr size_t k_max_msg = 4096;

namespace proto {}  // namespace proto
}  // namespace ownredis