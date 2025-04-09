// local libs
#include <Proto/proto.h>
#include <Proto/types.h>
// sys libs
#include <arpa/inet.h>
#include <assert.h>
#include <errno.h>
#include <netinet/ip.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

// c++ libs
#include <string>
#include <vector>

static void msg(const char *msg) { fprintf(stderr, "%s\n", msg); }

static void die(const char *msg) {
  int err = errno;
  fprintf(stderr, "[%d] %s\n", err, msg);
  abort();
}

static int32_t read_full(int fd, char *buf, size_t n) {
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

// const size_t k_max_msg = 4096;

// enum {
//   TAG_NIL = 0,  // nil
//   TAG_ERR = 1,  // error code + msg
//   TAG_STR = 2,  // string
//   TAG_INT = 3,  // int64
//   TAG_DBL = 4,  // double
//   TAG_ARR = 5,  // array
// };

static int32_t print_response(const uint8_t *data, size_t size) {
  if (size < 1) {
    msg("bad response");
    return -1;
  }
  switch (data[0]) {
    case ownredis::TAG_NIL:
      printf("(nil)\n");
      return 1;
    case ownredis::TAG_ERR:
      if (size < 1 + 8) {
        msg("bad response");
        return -1;
      }
      {
        int32_t code = 0;
        uint32_t len = 0;
        memcpy(&code, &data[1], 4);
        memcpy(&len, &data[1 + 4], 4);
        if (size < 1 + 8 + len) {
          msg("bad response");
          return -1;
        }
        printf("(err) %d %.*s\n", code, len, &data[1 + 8]);
        return 1 + 8 + len;
      }
    case ownredis::TAG_STR:
      if (size < 1 + 4) {
        msg("bad response");
        return -1;
      }
      {
        uint32_t len = 0;
        memcpy(&len, &data[1], 4);
        if (size < 1 + 4 + len) {
          msg("bad response");
          return -1;
        }
        printf("(str) %.*s\n", len, &data[1 + 4]);
        return 1 + 4 + len;
      }
    case ownredis::TAG_INT:
      if (size < 1 + 8) {
        msg("bad response");
        return -1;
      }
      {
        int64_t val = 0;
        memcpy(&val, &data[1], 8);
        printf("(int) %llu\n", val);
        return 1 + 8;
      }
    case ownredis::TAG_DBL:
      if (size < 1 + 8) {
        msg("bad response");
        return -1;
      }
      {
        double val = 0;
        memcpy(&val, &data[1], 8);
        printf("(dbl) %g\n", val);
        return 1 + 8;
      }
    case ownredis::TAG_ARR:
      if (size < 1 + 4) {
        msg("bad response");
        return -1;
      }
      {
        uint32_t len = 0;
        memcpy(&len, &data[1], 4);
        printf("(arr) len=%u\n", len);
        size_t arr_bytes = 1 + 4;
        for (uint32_t i = 0; i < len; ++i) {
          int32_t rv = print_response(&data[arr_bytes], size - arr_bytes);
          if (rv < 0) {
            return rv;
          }
          arr_bytes += (size_t)rv;
        }
        printf("(arr) end\n");
        return (int32_t)arr_bytes;
      }
    default:
      msg("bad response");
      return -1;
  }
}

static int32_t read_res(int fd) {
  // 4 bytes header
  char rbuf[4 + ownredis ::k_max_msg + 1];
  errno = 0;
  int32_t err = read_full(fd, rbuf, 4);
  if (err) {
    if (errno == 0) {
      msg("EOF");
    } else {
      msg("read() error");
    }
    return err;
  }

  uint32_t len = 0;
  memcpy(&len, rbuf, 4);  // assume little endian
  if (len > ownredis::k_max_msg) {
    msg("too long");
    return -1;
  }

  // reply body
  err = read_full(fd, &rbuf[4], len);
  if (err) {
    msg("read() error");
    return err;
  }

  // print the result
  int32_t rv = print_response((uint8_t *)&rbuf[4], len);
  if (rv > 0 && (uint32_t)rv != len) {
    msg("bad response");
    rv = -1;
  }
  return rv;
}

int main(int argc, char **argv) {
  int fd = socket(AF_INET, SOCK_STREAM, 0);
  if (fd < 0) {
    die("socket()");
  }

  struct sockaddr_in addr = {};
  addr.sin_family = AF_INET;
  addr.sin_port = ntohs(1234);
  addr.sin_addr.s_addr = ntohl(INADDR_LOOPBACK);  // 127.0.0.1
  int rv = connect(fd, (const struct sockaddr *)&addr, sizeof(addr));
  if (rv) {
    die("connect");
  }

  std::vector<std::string> cmd;
  for (int i = 1; i < argc; ++i) {
    cmd.push_back(argv[i]);
  }

  int32_t err = proto::send_req(fd, cmd);
  if (err) {
    goto L_DONE;
  }
  err = read_res(fd);
  if (err) {
    goto L_DONE;
  }

L_DONE:
  close(fd);
  return 0;
}