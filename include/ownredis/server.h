#pragma ocne

#include <cstdint>
#include <string>
#include <vector>

namespace ownredis {
namespace server {

bool start_server(uint16_t port, const std::vector<std::string>& addrs =
                                     std::vector<std::string>{});
}

}  // namespace ownredis