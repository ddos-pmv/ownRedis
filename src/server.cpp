#include <iostream>
#include <cerrno>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>


static void die(const char * msg) {
    std::cerr << msg << ": " << std::strerror(errno) << '\n';
    abort();
}

static void msg(const char * msg) {
    std::cout << msg << '\n';
}

void doSomething(int connFd) {
    char buffer[64] = {};
    ssize_t n = read(connFd, buffer, sizeof(buffer) - 1);

    std::cout << n << '\n';

    if( n < 0 ) {
        msg("read() error");
        return;
    }

    std::cout << buffer << '\n';

    char wbuff[5] = {'w', 'o', 'r', 'l', 'd'};
    write(connFd, wbuff, sizeof(wbuff));
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
    if( rv < 0 ){
        die( "Error binding socket" );
    }


    //! Listen for incoming connections
    rv = listen(fd, SOMAXCONN);
    if( rv < 0 ){
        die("Error listening socket");
    }

    while( true ){
        //! Accept incoming connections
        sockaddr_in clientAddr{};
        socklen_t clientAddrLen = sizeof(clientAddr);

        int connFd = accept(fd, reinterpret_cast<sockaddr*>(&clientAddr), &clientAddrLen);
        if( connFd < 0 ) {
            continue;     //! Ignore errors
        }

        doSomething(connFd);
        close(connFd);
    }

    return 0;
}