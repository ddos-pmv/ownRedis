#include <ownredis/server.h>

#include <vector>
#include <string>
#include <iostream>

int main()
{
    std::cout << ownredis::server::start_server(1234, {});
    return 0;
}