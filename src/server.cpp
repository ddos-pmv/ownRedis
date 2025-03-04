// system
#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>
// C/C++
#include <unistd.h>

#include <cassert>
#include <cerrno>
#include <cstddef>
#include <deque>
#include <iostream>
#include <map>
#include <unordered_map>
// boost
#include <boost/container/devector.hpp>

#include "Protocol.h"
#include "hashtable.h"

#define container_of(ptr, T, member) ((T *)((char *)ptr - offsetof(T, member)))

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

struct Conn {
  int fd = -1;

  uint16_t port;
  char addrBuf[INET_ADDRSTRLEN];

  bool want_read = false;
  bool want_write = false;
  bool want_close = false;
  // buffered info
  boost::container::devector<uint8_t> incoming;  // data to be parsed by app
  boost::container::devector<uint8_t> outgoing;  // responses generate by app
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

static void buf_append(boost::container::devector<uint8_t> &buf,
                       const uint8_t *data, size_t len) {
  buf.insert(buf.end(), data, data + len);
}

static void buf_consume(boost::container::devector<uint8_t> &buf, size_t n) {
  buf.erase(buf.begin(), buf.begin() + n);
}

enum {
  RES_OK = 0,   // ok
  RES_ERR = 1,  // error
  RES_NX = 2,   // not found
};

struct Response {
  uint32_t status = 0;
  std::vector<uint8_t> data;
};

//    2
// +------+------+------+------+------+
// | nstr | len1 | str1 | len2 | str2 |
// +------+------+------+------+------+

static struct {
  HMap db;
} g_data;

struct Entry {
  struct HNode node;
  std::string key;
  std::string value;
};

static bool entry_eq(HNode *lhs, HNode *rhs) {
  Entry *le = container_of(lhs, Entry, node);
  Entry *re = container_of(rhs, Entry, node);
  return le->key == re->key;
}

// FNV hash
static uint64_t str_hash(const uint8_t *data, size_t len) {
  uint32_t h = 0x811c9dc5;
  for (int i = 0; i < len; i++) {
    h = (h + data[i]) * 0x01000193;
  }
  return h;
}

static void do_get(std::vector<std::string> &cmd, Response &resp) {
  Entry key;
  key.key.swap(cmd[1]);
  key.node.hcode = str_hash(reinterpret_cast<const uint8_t *>(key.key.data()),
                            key.key.size());

  HNode *node = hm_lookup(&g_data.db, &key.node, &entry_eq);

  if (!node) {
    resp.status = RES_NX;
    return;
  }

  // copy value
  const std::string &val = container_of(node, Entry, node)->value;
  assert(val.size() <= k_max_msg);
  resp.data.assign(val.begin(), val.end());
}

static void do_set(std::vector<std::string> &cmd, Response &resp) {
  Entry key;
  key.key.swap(cmd[1]);
  key.node.hcode = str_hash(reinterpret_cast<const uint8_t *>(key.key.data()),
                            key.key.size());
  HNode *node = hm_lookup(&g_data.db, &key.node, &entry_eq);

  if (node) {
    container_of(node, Entry, node)->value.swap(cmd[2]);
  } else {
    Entry *ent = new Entry;
    ent->key.swap(key.key);
    ent->node.hcode = key.node.hcode;
    ent->value.swap(cmd[2]);

    hm_insert(&g_data.db, &ent->node);
  }
}

static void do_del(std::vector<std::string> &cmd, Response &resp) {
  Entry key;
  key.key.swap(cmd[1]);
  key.node.hcode = str_hash(reinterpret_cast<const uint8_t *>(key.key.data()),
                            key.key.length());

  HNode *node = hm_delete(&g_data.db, &key.node, entry_eq);

  if (node) {
    delete container_of(node, Entry, node);
  } else {
    resp.status = RES_NX;
  }
}

static void do_request(std::vector<std::string> &cmd, Response &response) {
  if (cmd.size() == 2 && cmd[0] == "get") {
    do_get(cmd, response);
  } else if (cmd.size() == 3 && cmd[0] == "set") {
    do_set(cmd, response);
  } else if (cmd.size() == 2 && cmd[0] == "del") {
    do_del(cmd, response);
  } else {
    response.status = RES_ERR;  // unrecognized command
  }
}

static void make_response(Response &response,
                          boost::container::devector<uint8_t> &out) {
  uint32_t respLen = 4 + response.data.size();
  buf_append(out, reinterpret_cast<uint8_t *>(&respLen), 4);
  buf_append(out, reinterpret_cast<uint8_t *>(&response.status), 4);
  buf_append(out, response.data.data(), response.data.size());
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
  for (const auto &arg : cmd) std::cout << arg;
  std::cout << '\n';

  Response response;
  do_request(cmd, response);
  make_response(response, conn.outgoing);
  //! remove message
  buf_consume(conn.incoming, len + 4);

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
  buf_consume(conn.outgoing, rv);

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
  buf_append(conn.incoming, buf, rv);

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
