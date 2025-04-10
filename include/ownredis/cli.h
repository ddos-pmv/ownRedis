#pragma once

#include <cstdint>
#include <vector>

namespace ownredis {
namespace cli {

constexpr size_t k_max_msg = 4096;

int32_t send_req(int fd, const std::vector<std::string> &cmd);
size_t read_res(int fd, std::vector<std::string> &ans);

}  // namespace cli
}  // namespace ownredis