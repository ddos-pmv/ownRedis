#include <netinet/in.h>
#include <ownredis/cli.h>
#include <ownredis/server.h>
#include <sys/socket.h>
#include <sys/types.h>

#include <cassert>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

void print_cmd(const std::vector<std::string> &cmd) {
  for (int i = 0; i < cmd.size(); i++) {
    std::cout << cmd[i] << " ";
  }
  std::cout << '\n';
}
using namespace std::chrono_literals;
int main() {
  uint16_t server_port = 1234;

  std::thread server_th(
      [=]() { ownredis::server::start_server(server_port, {}); });
  std::cout << "SERVER PID:" << server_th.get_id() << '\n';

  std::this_thread::sleep_for(2000ms);
  int fd = socket(AF_INET, SOCK_STREAM, 0);

  assert(fd > 0);

  struct sockaddr_in addr = {};
  addr.sin_family = AF_INET;
  addr.sin_port = ntohs(server_port);
  addr.sin_addr.s_addr = ntohl(INADDR_LOOPBACK);
  int conn_fd =
      connect(fd, reinterpret_cast<const sockaddr *>(&addr), sizeof(addr));

  if (conn_fd < 0) {
    std::cerr << std::strerror(errno);
    std::terminate();
  }

  static std::vector<std::string> req = {"set", "hello", "world"};
  std::cout << req.size();
  ownredis::cli::send_req(fd, req);
  std::cout << "[client]:";
  print_cmd(req);

  static std::vector<std::string> resp = {};
  ownredis::cli::read_res(fd, resp);
  std::cout << "[server]:";
  print_cmd(resp);

  req.clear();
  req = {"get", "hello"};
  ownredis::cli::send_req(fd, req);
  std::cout << "[client]:";
  print_cmd(req);

  resp.clear();
  ownredis::cli::read_res(fd, resp);
  std::cout << "[server]:";
  print_cmd(resp);

  server_th.join();
  return 0;
}