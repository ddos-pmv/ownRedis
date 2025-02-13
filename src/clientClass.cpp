#include <iostream>
#include "TcpClient.h"

int main()
{
    TcpClient client("127.0.0.1", 1234);
    client.start();

    // if ( client.ping() )
    client.tcpWrite("hello");
    std::string ans = client.tcpRead();
    while ( !ans.empty() )
    {
        std::cout << ans.length() << '\n';
        ans = client.tcpRead();
    }
    client.tcpWrite("adfadfadfaasdfadfafdasdfasd");

    std::cerr << client.error();
    return 0;
}
