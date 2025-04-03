// system
#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>
// My libs
#include <Proto/buf_utils.h>
#include <Proto/out_utils.h>
#include <Proto/types.h>
#include <hashtable.h>
#include <utils.h>
#include <zset.h>

// #include <boost/container/devector.hpp>
// C/C++
#include <cassert>
#include <cerrno>
#include <cstddef>
#include <deque>
#include <iostream>
#include <map>
#include <unordered_map>

#include "Protocol.h"

using ZSet = ownredis::ZSet;
using Buffer = ownredis::Buffer;
using ZNode = ownredis::ZNode;
using HNode = ownredis::HNode;
using HMap = ownredis::HMap;

const size_t k_max_msg = 32 << 20;
const size_t k_max_args = 200 * 1000;

static void die(const char *msg) {
  std::cerr << msg << ": " << std::strerror(errno) << '\n';
  abort();
}

static void msg_errno(const char *msg) {
  std::cerr << "errno: " << std::strerror(errno) << ". " << msg << '\n';
}

static void msg(const char *msg) { std::cout << msg << '\n'; }

static void fd_set_nb(int fd) {
  errno = 0;
  int flags = fcntl(fd, F_GETFL, 0);
  if (errno) {
    die("fcntl() error");
    return;
  }

  errno = 0;
  flags = fcntl(fd, F_SETFL, flags | O_NONBLOCK);
  if (errno) {
    die("fcntl() error");
    return;
  }
}

struct Response {
  uint32_t status = 0;
  Buffer data;
};

struct Conn {
  int fd = -1;

  uint16_t port;
  char addrBuf[INET_ADDRSTRLEN];

  bool want_read = false;
  bool want_write = false;
  bool want_close = false;
  // buffered info
  Buffer incoming;  // data to be parsed by app
  Buffer outgoing;  // responses generate by app
};

static std::unique_ptr<Conn> handle_accept(int fd) {
  //! accept
  sockaddr_in client_addr = {};
  socklen_t socklen = sizeof(client_addr);
  int connFd = accept(fd, reinterpret_cast<sockaddr *>(&client_addr), &socklen);
  if (connFd < 0) {
    msg_errno("accept() error");
    return nullptr;
  }

  std::cout << "[New connection] " << inet_ntoa(client_addr.sin_addr) << ":"
            << ntohs(client_addr.sin_port) << '\n';

  //! set new connection fd to nonblocking mode
  fd_set_nb(connFd);

  std::unique_ptr<Conn> conn(new Conn());
  conn->port = ntohs(client_addr.sin_port);
  inet_ntop(AF_INET, &client_addr.sin_addr, conn->addrBuf, INET_ADDRSTRLEN);
  conn->fd = connFd;
  conn->want_read = true;

  return conn;
}

static struct {
  HMap db;
} g_data;

enum { T_INIT = 0, T_STR = 1, T_ZSET = 2 };

struct Entry {
  HNode node;
  std::string key;
  // value
  uint32_t type = 0;
  // one of the following
  std::string str;
  ZSet zset;
};

static Entry *entry_new(uint32_t type) {
  Entry *ent = new Entry();
  ent->type = type;
  return ent;
}

static void entry_del(Entry *ent) {
  if (ent->type == T_ZSET) {
    zset_clear(&ent->zset);
  }
  delete ent;
}

struct LookupKey {
  HNode node;
  std::string key;
};

static bool entry_eq(HNode *lhs, HNode *rhs) {
  Entry *le = container_of(lhs, Entry, node);
  Entry *re = container_of(rhs, Entry, node);
  return le->key == re->key;
}

static void response_begin(Buffer &buf, size_t *header) {
  *header = buf.size();
  ownredis::proto::buf_append_u32(buf, 0);
}

static size_t response_size(Buffer &buf, size_t header) {
  return buf.size() - header - 4;
}

static void response_end(Buffer &buf, size_t header) {
  size_t msg_len = response_size(buf, header);
  if (msg_len > k_max_msg) {
    buf.resize(
        boost::container::devector<unsigned char>::size_type(header + 4));
    ownredis::proto::out_err(buf, ownredis::ERR_TOO_BIG, "Message too large");
    msg_len = response_size(buf, header);
  }

  auto len = static_cast<uint32_t>(msg_len);
  std::memcpy(&buf[header], &len, 4);
}

static void do_get(std::vector<std::string> &cmd, Buffer &buf) {
  LookupKey key;
  key.key.swap(cmd[1]);
  key.node.hcode = str_hash(reinterpret_cast<const uint8_t *>(key.key.data()),
                            key.key.size());

  HNode *node = hm_lookup(&g_data.db, &key.node, &entry_eq);
  if (!node) {
    return ownredis::proto::out_nil(buf);
  }

  // copy value
  Entry *ent = container_of(node, Entry, node);
  if (ent->type != T_STR) {
    ownredis::proto::out_err(buf, ownredis::ERR_BAD_TYPE, "not a string value");
  };
  return ownredis::proto::out_str(buf, ent->str.data(), ent->str.length());
}

static void do_set(std::vector<std::string> &cmd, Buffer &buf) {
  LookupKey key;
  key.key.swap(cmd[1]);
  key.node.hcode = str_hash(reinterpret_cast<const uint8_t *>(key.key.data()),
                            key.key.size());
  HNode *node = hm_lookup(&g_data.db, &key.node, &entry_eq);

  if (node) {
    Entry *ent = container_of(node, Entry, node);
    if (ent->type != T_STR) {
      return ownredis::proto::out_err(buf, ownredis::ERR_BAD_TYPE,
                                      "not a string value exists");
    }
    ent->str.swap(cmd[2]);
  } else {
    Entry *ent = entry_new(T_STR);
    ent->key.swap(key.key);
    ent->node.hcode = key.node.hcode;
    ent->str.swap(cmd[2]);
    hm_insert(&g_data.db, &ent->node);
  }
  return ownredis::proto::out_nil(buf);
}

static void do_del(std::vector<std::string> &cmd, Buffer &buf) {
  LookupKey key;
  key.key.swap(cmd[1]);
  key.node.hcode = str_hash(reinterpret_cast<const uint8_t *>(key.key.data()),
                            key.key.length());

  HNode *node = hm_delete(&g_data.db, &key.node, entry_eq);

  if (node) {
    entry_del(container_of(node, Entry, node));
  }
  return ownredis::proto::out_int(buf, node ? 1 : 0);
}

static bool cb_keys(HNode *nodeP, void *arg) {
  if (nodeP == nullptr) {
    msg("wrong buffer in cb_keys");
    return false;
  }

  Buffer &out = *reinterpret_cast<Buffer *>(arg);
  Entry *entPtr = container_of(nodeP, Entry, node);

  std::cout << entPtr->key.length();
  std::cout << entPtr->key;
  const std::string &key = (container_of(nodeP, Entry, node)->key);
  ownredis::proto::out_str(out, key.data(), key.size());
  return true;
}

static void do_keys(std::vector<std::string> &, Buffer &buf) {
  ownredis::proto::out_arr(buf, static_cast<uint32_t>(hm_size(&g_data.db)));
  // std::cout << hm_size(&g_data.db);
  hm_foreach(&g_data.db, &cb_keys, reinterpret_cast<void *>(&buf));
}

static bool str2dbl(const std::string &s, double &out) {
  char *endp = nullptr;
  out = strtod(s.c_str(), &endp);
  return endp == (s.c_str() + s.size()) && !std::isnan(out);
}

static bool str2int(const std::string &s, int64_t &out) {
  char *endp = nullptr;
  out = strtol(s.c_str(), &endp, 10);
  return endp == (s.c_str() + s.size());
}

static void do_zadd(std::vector<std::string> &cmd, Buffer &buf) {
  double score;
  if (!str2dbl(cmd[2], score)) {
    return ownredis::proto::out_err(buf, ownredis::ERR_BAD_ARG,
                                    "expected double(float)");
  }

  LookupKey key;
  key.key.swap(cmd[1]);
  key.node.hcode =
      str_hash(reinterpret_cast<uint8_t *>(key.key.data()), key.key.size());
  HNode *hnode = hm_lookup(&g_data.db, &key.node, &entry_eq);

  Entry *ent = nullptr;
  if (!hnode) {
    ent = entry_new(T_ZSET);
    ent->key.swap(key.key);
    ent->node.hcode = key.node.hcode;
    hm_insert(&g_data.db, &ent->node);
  } else {
    ent = container_of(hnode, Entry, node);
    if (ent->type != T_ZSET) {
      return ownredis::proto::out_err(buf, ownredis::ERR_BAD_TYPE,
                                      "expect zset");
    }
  }

  std::string name = cmd[3];
  bool added = zset_insert(&ent->zset, name.data(), name.length(), score);
  return ownredis::proto::out_int(buf, (int64_t)added);
}

static const ZSet k_empty_zset;

static ZSet *expect_zset(std::string &s) {
  LookupKey key;
  key.key.swap(s);
  key.node.hcode =
      str_hash(reinterpret_cast<uint8_t *>(key.key.data()), key.key.length());

  HNode *hnode = hm_lookup(&g_data.db, &key.node, entry_eq);
  if (!hnode) {
    return (ZSet *)&k_empty_zset;
  }

  Entry *ent = container_of(hnode, Entry, node);
  return ent->type == T_ZSET ? &ent->zset : nullptr;
}

// zrem zset zname
static void do_zrem(std::vector<std::string> &cmd, Buffer &buf) {
  ZSet *zset = expect_zset(cmd[1]);
  if (!zset) {
    return ownredis::proto::out_err(buf, ownredis::ERR_BAD_TYPE, "expect zset");
  }
  std::string name = cmd[2];
  ZNode *node = zset_lookup(zset, name.data(), name.size());

  if (node) {
    zset_delete(zset, node);
  }
  return ownredis::proto::out_int(buf, node ? 1 : 0);
}

// zscore zset zname
static void do_zscore(std::vector<std::string> &cmd, Buffer &buf) {
  ZSet *zset = expect_zset(cmd[1]);
  if (!zset) {
    return ownredis::proto::out_err(buf, ownredis::ERR_BAD_TYPE, "expect zset");
  }

  std::string name = cmd[2];
  ZNode *node = zset_lookup(zset, name.data(), name.length());
  return node ? ownredis::proto::out_dbl(buf, node->score)
              : ownredis::proto::out_nil(buf);
}
// zquery zset score name offset limint
static void do_zquery(std::vector<std::string> &cmd, Buffer &buf) {
  double score;
  if (!str2dbl(cmd[2], score)) {
    return ownredis::proto::out_err(buf, ownredis::ERR_BAD_ARG,
                                    "expected fp number");
  }

  const std::string &name = cmd[3];
  int64_t offset = 0;
  int64_t limit = 0;

  if (!str2int(cmd[4], offset) || !str2int(cmd[5], limit)) {
    return ownredis::proto::out_err(buf, ownredis::ERR_BAD_ARG, "expected int");
  }

  ZSet *zset = expect_zset(cmd[1]);
  if (!zset) {
    return ownredis::proto::out_err(buf, ownredis::ERR_BAD_TYPE,
                                    "expected zset");
  }

  if (limit <= 0) {
    return ownredis::proto::out_int(buf, 0);
  }

  ZNode *znode = zset_seekge(zset, score, name.data(), name.size());
  znode = zset_offset(znode, offset);

  size_t ctx = ownredis::proto::out_begin_arr(buf);
  int64_t n = 0;
  while (znode && n < limit) {
    ownredis::proto::out_str(buf, znode->name, znode->len);
    ownredis::proto::out_dbl(buf, znode->score);
    znode = zset_offset(znode, +1);
    n += 2;
  }

  ownredis::proto::out_end_arr(buf, ctx, static_cast<uint32_t>(n));
}

static void do_request(std::vector<std::string> &cmd, Buffer &buf) {
  if (cmd.size() == 2 && cmd[0] == "get") {
    do_get(cmd, buf);
  } else if (cmd.size() == 3 && cmd[0] == "set") {
    do_set(cmd, buf);
  } else if (cmd.size() == 2 && cmd[0] == "del") {
    do_del(cmd, buf);
  } else if (cmd.size() == 1 && cmd[0] == "keys") {
    do_keys(cmd, buf);
  } else if (cmd.size() == 4 && cmd[0] == "zadd") {
    do_zadd(cmd, buf);
  } else if (cmd.size() == 3 && cmd[0] == "zrem") {
    do_zrem(cmd, buf);
  } else if (cmd.size() == 3 && cmd[0] == "zscore") {
    do_zscore(cmd, buf);
  } else if (cmd.size() == 6 && cmd[0] == "zquery") {
    do_zquery(cmd, buf);
  } else {
    ownredis::proto::out_err(buf, ownredis::ERR_UNKNOWN, "unknown command");
  }
}

static bool try_one_request(Conn &conn) {
  if (conn.incoming.size() < 4) {
    return false;
  }

  uint32_t len = 0;
  std::memcpy(&len, conn.incoming.data(), 4);
  if (len > k_max_msg) {
    msg("too long msg (greater then k_max_msg)");
    conn.want_close = true;
    return false;
  }

  if (len + 4 > conn.incoming.size()) {
    msg("need read more");
    return false;
  }

  const uint8_t *request = &conn.incoming[4];

  std::vector<std::string> cmd;  // cmd exmaple: set [key] [value]
  if (Protocol::parse_request(request, cmd, len) < 0) {
    msg("bad request");
    conn.want_close = true;
    return false;  // error parsing
  }

  std::cout << "[client:" << conn.port << "]: ";
  for (const auto &arg : cmd) std::cout << arg << ' ';
  std::cout << '\n';

  size_t header_pos = 0;
  response_begin(conn.outgoing, &header_pos);
  do_request(cmd, conn.outgoing);
  response_end(conn.outgoing, header_pos);

  //! remove message
  ownredis::proto::buf_consume(conn.incoming, len + 4);

  return true;
}

static void handle_write(Conn &conn) {
  assert(conn.outgoing.size() > 0);
  ssize_t rv = write(conn.fd, conn.outgoing.data(), conn.outgoing.size());

  if (rv < 0) {
    if (errno == EINTR) {
      return;  //! not ready yet
    }
    if (errno == EPIPE) {
      msg_errno("write() broken pipe, fd closed");
      conn.want_close = true;
      return;
    }

    msg_errno("write() error");
    conn.want_close = true;
    return;
  }

  //! remove sended messgae
  ownredis::proto::buf_consume(conn.outgoing, rv);

  if (conn.outgoing.size() == 0) {
    conn.want_read = true;
    conn.want_write = false;
  }
}

static void handle_read(Conn &conn) {
  uint8_t buf[64 * 1024];
  ssize_t rv = read(conn.fd, buf, sizeof(buf));
  if (rv < 0 && (errno == EINTR || errno == EWOULDBLOCK || errno == EAGAIN)) {
    return;  //! not ready
  }

  if (rv < 0 || errno == ECONNRESET) {
    msg_errno("read() error");
    conn.want_close = true;
    return;
  }

  if (rv == 0) {
    if (conn.incoming.size() == 0) {
      std::string message("[client:" + std::to_string(conn.port) +
                          "] - closed");
      msg(message.c_str());
    } else {
      msg("unexpected EOF");
    }
    conn.want_close = true;
    return;
  }

  //! got new data
  ownredis::proto::buf_append(conn.incoming, buf, static_cast<size_t>(rv));

  //! handel requests line...
  while (try_one_request(conn)) {
  };

  if (conn.outgoing.size() > 0) {
    conn.want_read = false;
    conn.want_write = true;
    return handle_write(conn);
  }
}

int main() {
  signal(SIGPIPE, SIG_IGN);

  int fd = socket(AF_INET, SOCK_STREAM, 0);
  if (fd < 0) {
    die("Error creating socket");
  }

  //! Set socket options for reuse after restart
  int reuseSockVal = 1;
  setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &reuseSockVal, sizeof(reuseSockVal));

  //! Bind the socket to a port
  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = htonl(0);
  addr.sin_port = htons(1234);

  int rv = bind(fd, reinterpret_cast<sockaddr *>(&addr), sizeof(addr));
  if (rv < 0) {
    die("Error binding socket");
  }

  //! set listen nonblocking
  fd_set_nb(fd);

  //! Listen for incoming connections
  rv = listen(fd, SOMAXCONN);
  if (rv < 0) {
    die("listen() error");
  }

  //! a map of all client connections, keyed by fd
  std::vector<Conn *> fd2conn;
  std::unordered_map<int, std::unique_ptr<Conn> > fd2connMap;

  //! the event loop
  std::vector<pollfd> poll_args;
  while (true) {
    poll_args.clear();

    poll_args.emplace_back(pollfd{fd, POLLIN, 0});

    for (const auto &[key, conn] : fd2connMap) {
      if (!conn) {
        continue;
      }

      //! always poll fd for errof
      pollfd pfd{conn->fd, POLLERR, 0};

      if (conn->want_read) {
        pfd.events |= POLLIN;
      }

      if (conn->want_write) {
        pfd.events |= POLLOUT;
      }

      poll_args.emplace_back(pfd);
    }

    //! wait for readiness
    int rv = poll(poll_args.data(), static_cast<nfds_t>(poll_args.size()), 0);
    if (rv < 0 && errno == EINTR) {
      continue;  //! not an error
    }

    if (rv < 0) {
      die("poll() error");
    }

    //! handle the listening socket
    if (poll_args[0].revents) {
      if (std::unique_ptr<Conn> conn = std::move(handle_accept(fd))) {
        //! put it into the map
        assert(!fd2connMap[conn->fd]);
        fd2connMap[conn->fd] = std::move(conn);
      }
    }

    for (size_t i = 1; i < poll_args.size(); i++) {
      short ready = poll_args[i].revents;
      if (ready == 0) {
        continue;
      }

      Conn &conn = *fd2connMap[poll_args[i].fd];

      if (ready & POLLIN) {
        assert(conn.want_read);
        handle_read(conn);
      }

      if (ready & POLLOUT) {
        assert(conn.want_write);
        handle_write(conn);
      }

      if (ready & POLLERR || conn.want_close) {
        close(conn.fd);
        fd2connMap.erase(conn.fd);
      }
    }
  }
  return 0;
}