//
// Created by Сергей Перлин on 13.02.2025.
//

#ifndef TCP_CLIENT_H
#define TCP_CLIENT_H

#include <string>
#include <vector>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/fcntl.h>
#include <unistd.h>

class TcpClient {
public:
    explicit TcpClient(const std::string &addr, uint16_t port);

    ~TcpClient();

    void start();

    bool ping();

    void stop();

    std::string error() const;

    bool tcpWrite(const std::string &str);

    std::string tcpRead(size_t sizeToRead = 0);

    int fd() const;

    sockaddr_in sockAddr() const;

protected:
    int _fd;
    sockaddr_in _sockAddr{};
    std::string _error;

    ssize_t getReadBufSize();

    bool setNonBlocking();
};


#endif //TCP_CLIENT_H
