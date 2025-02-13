#include <benchmark/benchmark.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <cstring>
#include <iostream>

#include "../src/TcpClient.h"

static void BM_Query(benchmark::State &state)
{
    TcpClient client("127.0.0.1", 1234);
    TcpClient client2("127.0.0.1", 1234);
    client.start();
    client2.start();
    for ( auto _: state )
    {
        client.tcpWrite("01234567890123456789012345678901234567890123456789");
        client2.tcpWrite("abcdefg");
        int count = 54;
        // int count2 = 11;
        while ( count || 0 )
        {
            count -= client.tcpRead(9).size();
            // count2 -= client2.tcpRead(11).size();
        }
    }

    client.stop();
}

BENCHMARK(BM_Query);

BENCHMARK_MAIN();
