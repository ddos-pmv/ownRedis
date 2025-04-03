#pragma once

#include <arpa/inet.h>
#include <netinet/ip.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cassert>
#include <iostream>
#include <string>
#include <vector>

#include "types.h"

namespace ownredis {
namespace proto {
namespace {

void msg(const char *msg) { std::cerr << msg << std::endl; }
int32_t write_all(int fd, const char *buf, size_t n) {
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
int32_t read_full(int fd, char *buf, size_t n) {
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
}  // namespace

// incoming/outgoing buffer example
//      4b     // +---4b--+--4b--+------+------+------+
// len of msg  // |  nstr | len1 | str1 | len2 | str2 |
//             // +-------+------+------+------+------+
int32_t send_req(int fd, const std::vector<std::string> &cmd) {
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
  return write_all(fd, wbuf, 4 + len);
}

int32_t read_res(int fd, std::vector<std::string> cmd) {
  char rbuf[4 + k_max_msg + 1];
  errno = 0;
  int32_t err = read_full(fd, rbuf, 4);
  if (err) {
    if (errno == 0)
      msg("EOF");
    else
      msg("read() error");
    return err;
  }

  uint32_t len = 0;
  std::memcpy(&len, rbuf, 4);  // assume little endian
  if (len > k_max_msg) {
    msg("too long");
    return -1;
  }

  err = read_full(fd, &rbuf[4], len);
  if (err) {
    msg("read() error");
    return err;
  }
}

}  // namespace proto

}  // namespace ownredis