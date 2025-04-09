#include "ownredis/cli.h"

#include <Proto/proto.h>
#include <Proto/types.h>
#include <utils.h>

namespace ownredis {
// incoming/outgoing buffer example
// -----4b-----+---4b--+--4b--+------+------+------+
// len of msg  |  nstr | len1 | str1 | len2 | str2 |
// ----------- +-------+------+------+------+------+
//---------------------+----cmd1-----+-----arg1----+
int32_t cli::send_req(int fd, const std::vector<std::string> &cmd) {
  uint32_t len = 4;
  for (const std::string &s : cmd) {
    len += 4 + s.size();
  }
  if (len > k_max_msg) {
    return -1;
  }

  char wbuf[4 + k_max_msg];
  memcpy(&wbuf[0], &len, 4);  // assume little endian
  uint32_t n = (uint32_t)cmd.size();
  memcpy(&wbuf[4], &n, 4);
  size_t cur = 8;
  for (const std::string &s : cmd) {
    uint32_t p = (uint32_t)s.size();
    memcpy(&wbuf[cur], &p, 4);
    memcpy(&wbuf[cur + 4], s.data(), s.size());
    cur += 4 + s.size();
  }
  return proto::write_all(fd, wbuf, 4 + len);
}
size_t cli::read_res(int fd, std::vector<std::string> &ans) {
  size_t start_index = ans.size() - 1;
  char rbuf[4 + k_max_msg + 1];
  errno = 0;
  int32_t err = proto::read_full(fd, rbuf, 4);
  if (err) {
    if (errno == 0) {
      msg("EOF");
      //   ans.push_back("EOF");
    } else {
      msg_errno("read() error");
      //   ans.push_back("read() error");
    }
    return -1;
  }

  uint32_t len = 0;
  memcpy(&len, rbuf, 4);  // assume little endian
  if (len > k_max_msg) {
    msg("too long");
    return -1;
  }

  // reply body
  err = proto::read_full(fd, &rbuf[4], len);
  if (err) {
    msg("read() error");
    return -1;
  }

  if (!proto::parse_response(reinterpret_cast<uint8_t *>(&rbuf[4]), ans, len)) {
    msg("parse error");
    return -1;
  }
  return start_index;
}

}  // namespace ownredis
