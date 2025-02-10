#include <benchmark/benchmark.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <cstring>
#include <iostream>


const size_t k_max_msg = 4096;

static void die(const char * msg)
{
    std::cerr << msg << ": " << std::strerror(errno) << '\n';
    abort();
}

static int32_t read_full(int fd, char * buf, size_t sizeToRead)
{
    while ( sizeToRead > 0 )
    {
        ssize_t rv = read(fd, buf, sizeToRead);
        if ( rv <= 0 ) return -1;
        buf += rv;
        sizeToRead -= rv;
    }
    return 0;
}

static int32_t write_full(int fd, const char * buf, size_t sizeToWrite)
{
    while ( sizeToWrite > 0 )
    {
        ssize_t rv = write(fd, buf, sizeToWrite);
        if ( rv <= 0 ) return -1;
        buf += rv;
        sizeToWrite -= rv;
    }
    return 0;
}

static int32_t query(int fd, const char * text)
{
    char wbuf[k_max_msg];
    uint32_t len = std::strlen(text);
    if ( len > k_max_msg ) return -1;

    std::memcpy(wbuf, &len, 4);
    std::memcpy(wbuf + 4, text, len);

    return write_full(fd, wbuf, len + 4);
}

static void BM_Query(benchmark::State &state)
{
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if ( fd < 0 ) die("Error creating socket");

    sockaddr_in sockAddr{};
    sockAddr.sin_family = AF_INET;
    sockAddr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    sockAddr.sin_port = htons(1234);

    if ( connect(fd, reinterpret_cast<sockaddr *>(&sockAddr), sizeof( sockAddr )) < 0 )
    {
        die("Error connecting");
    }

    char rbuf[k_max_msg];

    for ( auto _: state )
    {
        for ( ssize_t i = 0; i < state.range(0); i++ ) query(fd, "hello");
        read_full(fd, rbuf, 4); // Читаем длину ответа
        uint32_t resp_len;
        std::memcpy(&resp_len, rbuf, 4);
        read_full(fd, rbuf, resp_len); // Читаем сам ответ
    }

    close(fd);
}

BENCHMARK(BM_Query)->Arg(4);

BENCHMARK_MAIN();
