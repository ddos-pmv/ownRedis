#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
const size_t k_max_msg = 4096;

static void die(const char * msg)
{
    std::cerr << msg << ": " << std::strerror(errno) << '\n';
    abort();
}

static void msg(const char * msg)
{
    std::cout << msg << '\n';
}

static int32_t read_full(int fd, char * buf, size_t sizeToRead)
{
    while ( sizeToRead > 0 )
    {
        ssize_t rv = read(fd, buf, sizeToRead);
        if ( rv <= 0 )
        {
            return -1;
        }
        if ( rv > sizeToRead )
        {
            return -1;
        }
        // assert( rv <= sizeToRead );
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
        if ( rv <= 0 )
        {
            return -1;
        }
        if ( rv > sizeToWrite )
        {
            return -1;
        }

        // assert( rv <= sizeToWrite );
        buf += rv;
        sizeToWrite -= rv;
    }
    return 0;
}

static int32_t query(int fd, const char * text)
{
    char wbuf[k_max_msg];

    uint32_t len = std::strlen(text);
    if ( len > k_max_msg )
    {
        msg("too long");
        return -1;
    }

    std::memcpy(wbuf, &len, 4);
    std::memcpy(wbuf + 4, text, len);

    int32_t err = write_full(fd, wbuf, len + 4);

    return err;
}

int main()
{
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if ( fd < 0 )
    {
        die("Error creating socket");
    }

    sockaddr_in sockAddr{};
    sockAddr.sin_family = AF_INET;
    sockAddr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    sockAddr.sin_port = htons(1234);
    int rv = connect(fd, reinterpret_cast<sockaddr *>(&sockAddr), sizeof( sockAddr ));
    if ( rv < 0 )
    {
        die("Error connecting");
    }

    int32_t err = query(fd, "hello");
    if ( err )
    {
        close(fd);
        die("query() error");
    }
    sleep(3);
    err = query(fd, "hello2");
    close(fd);
    if ( err )
    {
        die("query() error");
    }
    return 0;
}
