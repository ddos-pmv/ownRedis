#pragma once

#include <utils.h>

namespace ownredis {
namespace cli {

int32_t send_req(int fd, const std::vector<std::string> &cmd);
size_t read_res(int fd, std::vector<std::string> &ans);

}  // namespace cli
}  // namespace ownredis