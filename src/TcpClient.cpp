//
// Created by Сергей Перлин on 13.02.2025.
//

#include "TcpClient.h"

#include <sys/fcntl.h>

TcpClient::TcpClient(const std::string &addr, uint16_t port) : _error("no errors")
{
    errno = 0;
    _fd = socket(AF_INET, SOCK_STREAM, 0);

    if ( _fd < 0 ) _error = "socket()";

    _sockAddr.sin_family = AF_INET;
    _sockAddr.sin_port = htons(port);
    inet_pton(AF_INET, addr.c_str(), &_sockAddr.sin_addr.s_addr);
}

TcpClient::~TcpClient()
{
    if ( ping() ) close(_fd);
}


void TcpClient::start()
{
    errno = 0;
    int rv = connect(_fd, reinterpret_cast<sockaddr *>(&_sockAddr), sizeof( _sockAddr ));
    if ( rv < 0 )
    {
        _error = "connect()";
    }

    setNonBlocking();
}


bool TcpClient::ping()
{
    errno = 0;
    char buf[1] = {0};
    ssize_t bytes_sent = send(_fd, buf, sizeof( buf ), MSG_NOSIGNAL);
    if ( bytes_sent < 0 )
    {
        if ( errno == EPIPE )
        {
            return false; // Соединение закрыто
        }
        _error = "ping():";
        return false; // Другая ошибка
    }
    return true;
}

void TcpClient::stop()
{
    if ( ping() ) close(_fd);
}

std::string TcpClient::error() const
{
    return std::string(_error + " " + std::strerror(errno));
}

bool TcpClient::tcpWrite(const std::string &str)
{
    uint32_t sizeToWrite = str.length();
    if ( write(_fd, &sizeToWrite, 4) != 4 )
    {
        return false;
    };

    size_t iter = 0;

    while ( sizeToWrite > 0 )
    {
        ssize_t rv = write(_fd, str.data() + iter, sizeToWrite);
        if ( rv <= 0 )
        {
            _error = std::move(std::string("write() " + std::to_string(iter)));
            return false;
        }
        iter += rv;
        sizeToWrite -= rv;
    }

    return true;
}

std::string TcpClient::tcpRead(size_t sizeToRead)
{
    if ( sizeToRead == 0 ) sizeToRead = getReadBufSize();
    if ( sizeToRead == 0 ) return "";

    std::vector<char> buffer(sizeToRead);

    // Читаем данные из сокета
    ssize_t bytesRead = read(_fd, buffer.data(), buffer.size());
    if ( bytesRead < 0 )
    {
        if ( errno == EAGAIN || errno == EWOULDBLOCK )
        {
            // Нет данных для чтения
            _error = "read()";
            return "";
        }
        _error = "read(): ";
        return "";
    }

    // Возвращаем данные в виде строки
    return std::string(buffer.data(), bytesRead);
}


int TcpClient::fd() const
{
    return _fd;
}

sockaddr_in TcpClient::sockAddr() const
{
    return _sockAddr;
}

ssize_t TcpClient::getReadBufSize()
{
    size_t bufferSize = 0;
    socklen_t len = sizeof( bufferSize );
    if ( getsockopt(_fd, SOL_SOCKET, SO_RCVBUF, &bufferSize, &len) < 0 )
    {
        _error = "getsockopt()";
        return -1; // Ошибка
    }
    return bufferSize;
}

[[nodiscard]] bool TcpClient::setNonBlocking()
{
    int flags = fcntl(_fd, F_GETFL, 0);
    if ( flags == -1 )
    {
        _error = "fcntl(F_GETFL):";
        return false;
    }
    if ( fcntl(_fd, F_SETFL, flags | O_NONBLOCK) == -1 )
    {
        _error = "fcntl(F_SETFL):";
        return false;
    }
    return true;
};
