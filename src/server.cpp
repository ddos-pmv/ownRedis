#include <cassert>
#include <iostream>
#include <cerrno>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>

const size_t k_max_msg = 4096;

static void die(const char * msg) {
    std::cerr << msg << ": " << std::strerror(errno) << '\n';
    abort();
}

static void msg(const char * msg) {
    std::cout << msg << '\n';
}

static int32_t read_full(int fd, char * buf, size_t sizeToRead) {
    while ( sizeToRead > 0 ) {
        ssize_t rv = read(fd, buf, sizeToRead);
        if ( rv <= 0 ) {
            return -1;
        }
        if ( rv > sizeToRead ) {
            return -1;
        }
        // assert( rv <= sizeToRead );
        buf += rv;
        sizeToRead -= rv;
    }
    return 0;
}

static int32_t write_full(int fd, const char * buf, size_t sizeToWrite) {
    while ( sizeToWrite > 0 ) {
        ssize_t rv = write(fd, buf, sizeToWrite);
        if ( rv <= 0 ) {
            return -1;
        }
        if ( rv > sizeToWrite ) {
            return -1;
        }
        // assert( rv <= sizeToWrite );
        buf += rv;
        sizeToWrite -= rv;
    }
    return 0;
}

static int32_t one_request(int fd) {
    char rbuf[4 + k_max_msg];
    errno = 0;
    int32_t err = read_full(fd, rbuf, 4);
    std::cout << rbuf << '\n';
    if( err ) {
        msg(errno == 0 ? "EOF" : "Error reading");
        return err;
    }

    uint32_t len;
    std::memcpy(&len, rbuf, 4);  // assume little endian
    if(len > k_max_msg) {
        msg("too long msg");
        return -1;
    }

    // request body
    err = read_full(fd, rbuf + 4, len);
    if( err ) {
        msg("read() error");
        return -1;
    }

    std::cout << "client says: " << std::string(rbuf+4, len);

    const char * reply = "world";
    len = std::strlen(reply);
    char wbuf[4 + len];
    std::memcpy(wbuf, &len, 4);
    std::memcpy(wbuf + 4, reply, len);

    return write_full(fd, wbuf, 4 + len);
}
int main() {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if ( fd < 0 ) {
        die("Error creating socket");
    }

    //! Set socket options for reuse after restart
    int reuseSockVal = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &reuseSockVal, sizeof(reuseSockVal));


    //! Bind the socket to a port
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(0);
    addr.sin_port = htons(1234);

    int rv = bind(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr));
    if ( rv < 0 ) {
        die( "Error binding socket" );
    }


    //! Listen for incoming connections
    rv = listen(fd, SOMAXCONN);
    if( rv < 0 ){
        die("Error listening socket");
    }

    while ( true ) {
        //! Accept incoming connections
        sockaddr_in clientAddr{};
        socklen_t clientAddrLen = sizeof(clientAddr);

        int connFd = accept(fd, reinterpret_cast<sockaddr*>(&clientAddr), &clientAddrLen);
        if ( connFd < 0 ) {
            continue;     //! Ignore errors
        }

        while ( true ) {
            int32_t err = one_request(connFd);
            if ( err ) {
                break;
            }
        }

        close(connFd);
    }

    return 0;
}