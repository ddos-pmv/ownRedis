// system
#include <sys/socket.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <poll.h>
#include <arpa/inet.h>
// C/C++
#include <unistd.h>
#include <cassert>
#include <iostream>
#include <cerrno>

const size_t k_max_msg = 4096;

static void die(const char * msg)
{
    std::cerr << msg << ": " << std::strerror(errno) << '\n';
    abort();
}

static void msg_errno(const char * msg)
{
    std::cerr << "errno: " << std::strerror(errno) << ". " << msg << '\n';
}

static void msg(const char * msg)
{
    std::cout << msg << '\n';
}

static void fd_set_nb(int fd)
{
    errno = 0;
    int flags = fcntl(fd, F_GETFL, 0);
    if ( errno )
    {
        die("fcntl() error");
        return;
    }

    errno = 0;
    flags = fcntl(fd, F_SETFL, flags | O_NONBLOCK);
    if ( errno )
    {
        die("fcntl() error");
        return;
    }
}

struct Conn {
    int fd = -1;

    uint16_t port;
    char addrBuf[INET_ADDRSTRLEN];

    bool want_read = false;
    bool want_write = false;
    bool want_close = false;
    // buffered info
    std::vector<uint8_t> incoming; // data to be parsed by app
    std::vector<uint8_t> outgoing; // responses generate by app
};

static Conn * handle_accept(int fd)
{
    //! accept
    sockaddr_in client_addr = {};
    socklen_t socklen = sizeof( client_addr );
    int connFd = accept(fd, reinterpret_cast<sockaddr *>(&client_addr), &socklen);
    if ( connFd < 0 )
    {
        msg_errno("accept() error");
        return nullptr;
    }

    std::cout << "[New connection] " << inet_ntoa(client_addr.sin_addr)
            << ":" << ntohs(client_addr.sin_port) << '\n';

    //! set new connection fd to nonblocking mode
    fd_set_nb(connFd);

    Conn * conn = new Conn();
    conn->port = ntohs(client_addr.sin_port);
    inet_ntop(AF_INET, &client_addr.sin_addr, conn->addrBuf, INET_ADDRSTRLEN);
    conn->fd = connFd;
    conn->want_read = true;

    return conn;
}

// static int32_t write_full(int fd, const char * buf, size_t sizeToWrite)
// {
//     while ( sizeToWrite > 0 )
//     {
//         ssize_t rv = write(fd, buf, sizeToWrite);
//         if ( rv <= 0 )
//         {
//             return -1;
//         }
//         if ( rv > sizeToWrite )
//         {
//             return -1;
//         }
//         // assert( rv <= sizeToWrite );
//         buf += rv;
//         sizeToWrite -= rv;
//     }
//     return 0;
// }
static void buf_append(std::vector<uint8_t> &buf, const uint8_t * data, size_t len)
{
    buf.insert(buf.end(), data, data + len);
}

static void buf_consume(std::vector<uint8_t> &buf, size_t n)
{
    buf.erase(buf.begin(), buf.begin() + n);
}

static bool handle_one_line(Conn * conn)
{
    if ( conn->incoming.size() < 4 )
    {
        return false;
    }

    uint32_t len = 0;
    std::memcpy(&len, conn->incoming.data(), 4);

    if ( len > k_max_msg )
    {
        msg("too long msg (greater then k_max_msg)");
        conn->want_close = true;
        return false;
    }

    if ( len + 4 > conn->incoming.size() )
    {
        msg("wrong len of line");
        return false;
    }


    std::cout << "[client:" << conn->port << "]: " << std::string(conn->incoming.begin() + 4,
                                                                  conn->incoming.begin() + 4 + len) << '\n';

    //! generate response (echo)
    buf_append(conn->outgoing, reinterpret_cast<uint8_t *>(&len), 4);
    buf_append(conn->outgoing, &conn->incoming[4], len);

    //! remove message
    buf_consume(conn->incoming, len + 4);

    return true;
}

static void handle_write(Conn * conn)
{
    assert(conn->outgoing.size() > 0);
    ssize_t rv = write(conn->fd, conn->outgoing.data(), conn->outgoing.size());

    if ( rv < 0 )
    {
        if ( errno == EINTR )
        {
            return; //! not ready yet
        }
        if ( errno == EPIPE )
        {
            msg_errno("write() broken pipe, fd closed");
            conn->want_close = true;
            return;
        }

        msg_errno("write() error");
        conn->want_close = true;
        return;
    }

    //! remove sended messgae
    buf_consume(conn->outgoing, rv);

    if ( conn->outgoing.size() == 0 )
    {
        conn->want_read = true;
        conn->want_write = false;
    }
}

static void handle_read(Conn * conn)
{
    uint8_t buf[64 * 1024];
    ssize_t rv = read(conn->fd, buf, sizeof( buf ));
    if ( rv < 0 && ( errno == EINTR || errno == EWOULDBLOCK || errno == EAGAIN ) )
    {
        return; //! not ready
    }

    if ( rv < 0 || errno == ECONNRESET )
    {
        msg_errno("read() error");
        conn->want_close = true;
        return;
    }

    if ( rv == 0 )
    {
        if ( conn->incoming.size() == 0 )
        {
            std::string message("[client:" + std::to_string(conn->port) + "] - closed");
            msg(message.c_str());
        }
        else
        {
            msg("unexpected EOF");
        }
        conn->want_close = true;
        return;
    }

    //! got new data
    buf_append(conn->incoming, buf, rv);

    //! handel requests line...
    while ( handle_one_line(conn) )
    {
    };

    if ( conn->outgoing.size() > 0 )
    {
        conn->want_read = false;
        conn->want_write = true;
        return handle_write(conn);
    }
}

int main()
{
    signal(SIGPIPE, SIG_IGN);

    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if ( fd < 0 )
    {
        die("Error creating socket");
    }

    //! Set socket options for reuse after restart
    int reuseSockVal = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &reuseSockVal, sizeof( reuseSockVal ));

    //! Bind the socket to a port
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(0);
    addr.sin_port = htons(1234);

    int rv = bind(fd, reinterpret_cast<sockaddr *>(&addr), sizeof( addr ));
    if ( rv < 0 )
    {
        die("Error binding socket");
    }

    //! set listen nonblocking
    fd_set_nb(fd);

    //! Listen for incoming connections
    rv = listen(fd, SOMAXCONN);
    if ( rv < 0 )
    {
        die("listen() error");
    }

    //! a map of all client connections, keyed by fd
    std::vector<Conn *> fd2conn;
    std::unordered_map<int, Conn *> fd2connMap;

    //! the event loop
    std::vector<pollfd> poll_args;
    while ( true )
    {
        poll_args.clear();

        poll_args.emplace_back(pollfd{fd,POLLIN, 0});

        for ( const auto [key, conn]: fd2connMap )
        {
            if ( !conn )
            {
                continue;
            }

            //! always poll fd for errof
            pollfd pfd{conn->fd, POLLERR, 0};

            if ( conn->want_read )
            {
                pfd.events |= POLLIN;
            }

            if ( conn->want_write )
            {
                pfd.events |= POLLOUT;
            }

            poll_args.emplace_back(pfd);
        }

        //! wait for readiness
        int rv = poll(poll_args.data(), static_cast<nfds_t>(poll_args.size()), 0);
        if ( rv < 0 && errno == EINTR )
        {
            continue; //! not an error
        }

        if ( rv < 0 )
        {
            die("poll() error");
        }

        //! handle the listening socket
        if ( poll_args[0].revents )
        {
            if ( Conn * conn = handle_accept(fd) )
            {
                //! put it into the map
                assert(!fd2connMap[conn->fd]);
                fd2connMap[conn->fd] = conn;
            }
        }

        for ( size_t i = 1; i < poll_args.size(); i++ )
        {
            short ready = poll_args[i].revents;
            if ( ready == 0 )
            {
                continue;
            }

            Conn * conn = fd2connMap[poll_args[i].fd];

            if ( ready & POLLIN )
            {
                assert(conn->want_read);
                handle_read(conn);
            }

            if ( ready & POLLOUT )
            {
                assert(conn->want_write);
                handle_write(conn);
            }

            if ( ready & POLLERR || conn->want_close )
            {
                close(conn->fd);
                fd2connMap[conn->fd] = nullptr;
                delete conn;
            }
        }
    }
    return 0;
}
