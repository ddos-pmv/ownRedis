#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <cstring>
#include <iostream>
#include <chrono>


class Timer {
public:
    Timer() : m_bRunning(false)
    {
    };

    void start()
    {
        m_StartTime = std::chrono::system_clock::now();
        m_bRunning = true;
    }

    void stop()
    {
        m_EndTime = std::chrono::system_clock::now();
        m_bRunning = false;
    }

    double elapsedMilliseconds()
    {
        std::chrono::time_point<std::chrono::system_clock> endTime;

        if ( m_bRunning )
        {
            endTime = std::chrono::system_clock::now();
        }
        else
        {
            endTime = m_EndTime;
        }

        return std::chrono::duration_cast<std::chrono::milliseconds>(endTime - m_StartTime).count();
    }

    double elapsedSeconds()
    {
        return elapsedMilliseconds() / 1000.0;
    }

private:
    std::chrono::time_point<std::chrono::system_clock> m_StartTime;
    std::chrono::time_point<std::chrono::system_clock> m_EndTime;
    bool m_bRunning;
};

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
        std::cout << "read: " << rv << '\n';
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

int main()
{
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if ( fd < 0 ) die("Error creating socket");

    sockaddr_in sockAddr = {};
    sockAddr.sin_family = AF_INET;
    sockAddr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    sockAddr.sin_port = htons(1234);

    if ( connect(fd, reinterpret_cast<sockaddr *>(&sockAddr), sizeof( sockAddr )) < 0 )
    {
        die("Error connecting");
    }

    char rbuf[k_max_msg];

    Timer timer;
    timer.start();

    uint64_t bytesToRead = 0;
    for ( int i = 0; i < 200; i++ )
    {
        query(fd, "hello");
        bytesToRead += 9;
    }
    read_full(fd, rbuf, bytesToRead); // Читаем длину ответа
    std::cout << timer.elapsedMilliseconds();


    close(fd);

    return 0;
}
