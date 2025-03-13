#include <iostream>
#include "TcpClient.h"

int main()
{
    TcpClient client("127.0.0.1", 1234);
    client.start();

    // if ( client.ping() )
    client.tcpWrite("hello");
    client.tcpWrite("hello2");

    std::cerr << client.error();
    return 0;
}
