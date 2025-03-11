// system
#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>
// C/C++
#include <unistd.h>

#include <cassert>
#include <cerrno>
#include <cstddef>
#include <deque>
#include <iostream>
#include <map>
#include <unordered_map>
// boost
#include <boost/container/devector.hpp>

#include "Protocol.h"
#include "hashtable.h"

#define container_of(ptr, T, member) ((T *)((char *)ptr - offsetof(T, member)))

const size_t k_max_msg = 32 << 20;
const size_t k_max_args = 200 * 1000;

static void die ( const char * msg )
{
    std::cerr << msg << ": " << std::strerror( errno ) << '\n';
    abort();
}

static void msg_errno ( const char * msg )
{
    std::cerr << "errno: " << std::strerror( errno ) << ". " << msg << '\n';
}

static void msg ( const char * msg ) { std::cout << msg << '\n'; }

static void fd_set_nb ( int fd )
{
    errno = 0;
    int flags = fcntl( fd, F_GETFL, 0 );
    if ( errno )
    {
        die( "fcntl() error" );
        return;
    }

    errno = 0;
    flags = fcntl( fd, F_SETFL, flags | O_NONBLOCK );
    if ( errno )
    {
        die( "fcntl() error" );
        return;
    }
}

using Buffer = boost::container::devector<uint8_t>;

enum {
    TAG_NIL = 0,
    TAG_ERR = 1,
    TAG_STR = 2,
    TAG_INT = 3,
    TAG_DBL = 4,
    TAG_ARR = 5
};

enum {
    ERR_UNKNOWN = 0,
    ERR_TOO_BIG = 1
};

struct Response {
    uint32_t status = 0;
    Buffer data;
};

// incoming/outgoing buffer example
// +------+------+------+------+------+
// | nstr | len1 | str1 | len2 | str2 |
// +------+------+------+------+------+
struct Conn {
    int fd = -1;

    uint16_t port;
    char addrBuf[INET_ADDRSTRLEN];

    bool want_read = false;
    bool want_write = false;
    bool want_close = false;
    // buffered info
    Buffer incoming; // data to be parsed by app
    Buffer outgoing; // responses generate by app
};

static std::unique_ptr<Conn> handle_accept ( int fd )
{
    //! accept
    sockaddr_in client_addr = {};
    socklen_t socklen = sizeof( client_addr );
    int connFd = accept( fd, reinterpret_cast<sockaddr *>(&client_addr), &socklen );
    if ( connFd < 0 )
    {
        msg_errno( "accept() error" );
        return nullptr;
    }

    std::cout << "[New connection] " << inet_ntoa( client_addr.sin_addr ) << ":"
            << ntohs( client_addr.sin_port ) << '\n';

    //! set new connection fd to nonblocking mode
    fd_set_nb( connFd );

    std::unique_ptr<Conn> conn( new Conn() );
    conn->port = ntohs( client_addr.sin_port );
    inet_ntop( AF_INET, &client_addr.sin_addr, conn->addrBuf, INET_ADDRSTRLEN );
    conn->fd = connFd;
    conn->want_read = true;

    return conn;
}

static void buf_append ( Buffer & buf,
                         const uint8_t * data, size_t len )
{
    buf.insert( buf.end(), data, data + len );
}

static void buf_append_u8 ( Buffer & buf, const uint8_t data )
{
    buf.push_back( data );
}

static void buf_append_u32 ( Buffer & buf, const uint32_t data )
{
    buf.insert( buf.end(), reinterpret_cast<const uint8_t *>(&data), reinterpret_cast<const uint8_t *>(&data) + 4 );
}

static void buf_append_i64 ( Buffer & buf, const int64_t data )
{
    buf.insert( buf.end(), reinterpret_cast<const uint8_t *>(&data), reinterpret_cast<const uint8_t *>(&data) + 8 );
}

static void buf_append_dbl ( Buffer & buf, double data )
{
    buf_append( buf, (const uint8_t *) &data, 8 );
}

static void buf_consume ( Buffer & buf, size_t n )
{
    buf.erase( buf.begin(), buf.begin() + n );
}

static void out_nil ( Buffer & buf )
{
    buf_append_u8( buf, TAG_NIL );
}

static void out_str ( Buffer & out, const char * s, size_t len )
{
    buf_append_u8( out, TAG_STR );
    buf_append_u32( out, static_cast<uint32_t>(len) );
    buf_append( out, reinterpret_cast<const uint8_t *>(s), len );
}

static void out_int ( Buffer & out, int64_t val )
{
    buf_append_u8( out, TAG_INT );
    buf_append_i64( out, val );
}

static void out_arr ( Buffer & out, uint32_t n )
{
    buf_append_u8( out, TAG_ARR );
    buf_append_u32( out, n );
}

static void out_dbl ( Buffer & out, double val )
{
    buf_append_u8( out, TAG_DBL );
    buf_append_dbl( out, val );
}

static void out_err ( Buffer & out, uint32_t errCode, const std::string & msg )
{
    buf_append_u8( out, TAG_ERR );
    buf_append_u32( out, errCode );
    buf_append_u32( out, static_cast<uint32_t>(msg.size()) );
    buf_append( out, reinterpret_cast<const uint8_t *>(msg.data()), msg.size() );
}

static struct {
    HMap db;
} g_data;

struct Entry {
    struct HNode node;
    std::string key;
    std::string value;
};

static bool entry_eq ( HNode * lhs, HNode * rhs )
{
    Entry * le = container_of( lhs, Entry, node );
    Entry * re = container_of( rhs, Entry, node );
    return le->key == re->key;
}

// FNV hash
static uint64_t str_hash ( const uint8_t * data, size_t len )
{
    uint32_t h = 0x811c9dc5;
    for ( int i = 0; i < len; i++ )
    {
        h = ( h + data[i] ) * 0x01000193;
    }
    return h;
}

static void response_begin ( Buffer & buf, size_t * header )
{
    *header = buf.size();
    buf_append_u32( buf, 0 );
}

static size_t response_size ( Buffer & buf, size_t header )
{
    return buf.size() - header - 4;
}

static void response_end ( Buffer & buf, size_t header )
{
    size_t msg_len = response_size( buf, header );
    if ( msg_len > k_max_msg )
    {
        buf.resize( boost::container::devector<unsigned char>::size_type( header + 4 ) );
        out_err( buf, ERR_TOO_BIG, "Message too large" );
        msg_len = response_size( buf, header );
    }

    auto len = static_cast<uint32_t>( msg_len );
    std::memcpy( &buf[header], &len, 4 );
}

static void do_get ( std::vector<std::string> & cmd, Buffer & buf )
{
    Entry key;
    key.key.swap( cmd[1] );
    key.node.hcode = str_hash( reinterpret_cast<const uint8_t *>(key.key.data()),
                               key.key.size() );

    HNode * node = hm_lookup( &g_data.db, &key.node, &entry_eq );

    if ( !node )
    {
        return out_nil( buf );
    }

    // copy value
    const std::string & val = container_of( node, Entry, node )->value;
    return out_str( buf, val.data(), val.size() );
}

static void do_set ( std::vector<std::string> & cmd, Buffer & buf )
{
    Entry key;
    key.key.swap( cmd[1] );
    key.node.hcode = str_hash( reinterpret_cast<const uint8_t *>(key.key.data()),
                               key.key.size() );
    HNode * node = hm_lookup( &g_data.db, &key.node, &entry_eq );

    if ( node )
    {
        container_of( node, Entry, node )->value.swap( cmd[2] );
    }
    else
    {
        Entry * ent = new Entry;
        ent->key.swap( key.key );
        ent->node.hcode = key.node.hcode;
        ent->value.swap( cmd[2] );

        hm_insert( &g_data.db, &ent->node );
    }

    return out_nil(buf);
}

static void do_del ( std::vector<std::string> & cmd, Buffer & buf )
{
    Entry key;
    key.key.swap( cmd[1] );
    key.node.hcode = str_hash( reinterpret_cast<const uint8_t *>(key.key.data()),
                               key.key.length() );

    HNode * node = hm_delete( &g_data.db, &key.node, entry_eq );

    if ( node )
    {
        delete container_of( node, Entry, node );
    }

    return out_int( buf, node ? 1 : 0 );
}

static void do_request ( std::vector<std::string> & cmd, Buffer & buf)
{
    if ( cmd.size() == 2 && cmd[0] == "get" )
    {
        do_get( cmd, buf );
    }
    else if ( cmd.size() == 3 && cmd[0] == "set" )
    {
        do_set( cmd, buf );
    }
    else if ( cmd.size() == 2 && cmd[0] == "del" )
    {
        do_del( cmd, buf );
    }
    else
    {
        out_err( buf, ERR_TOO_BIG, "unknown command" );
    }
}

static bool try_one_request ( Conn & conn )
{
    if ( conn.incoming.size() < 4 )
    {
        return false;
    }

    uint32_t len = 0;
    std::memcpy( &len, conn.incoming.data(), 4 );
    if ( len > k_max_msg )
    {
        msg( "too long msg (greater then k_max_msg)" );
        conn.want_close = true;
        return false;
    }

    if ( len + 4 > conn.incoming.size() )
    {
        msg( "need read more" );
        return false;
    }

    const uint8_t * request = &conn.incoming[4];

    std::vector<std::string> cmd; // cmd exmaple: set [key] [value]
    if ( Protocol::parse_request( request, cmd, len ) < 0 )
    {
        msg( "bad request" );
        conn.want_close = true;
        return false; // error parsing
    }

    std::cout << "[client:" << conn.port << "]: ";
    for ( const auto & arg: cmd ) std::cout << arg << ' ';
    std::cout << '\n';


    size_t header_pos = 0;
    response_begin( conn.outgoing, &header_pos );
    do_request( cmd, conn.outgoing );
    response_end( conn.outgoing, header_pos );

    //! remove message
    buf_consume( conn.incoming, len + 4 );

    return true;
}

static void handle_write ( Conn & conn )
{
    assert( conn.outgoing.size() > 0 );
    ssize_t rv = write( conn.fd, conn.outgoing.data(), conn.outgoing.size() );

    if ( rv < 0 )
    {
        if ( errno == EINTR )
        {
            return; //! not ready yet
        }
        if ( errno == EPIPE )
        {
            msg_errno( "write() broken pipe, fd closed" );
            conn.want_close = true;
            return;
        }

        msg_errno( "write() error" );
        conn.want_close = true;
        return;
    }

    //! remove sended messgae
    buf_consume( conn.outgoing, rv );

    if ( conn.outgoing.size() == 0 )
    {
        conn.want_read = true;
        conn.want_write = false;
    }
}

static void handle_read ( Conn & conn )
{
    uint8_t buf[64 * 1024];
    ssize_t rv = read( conn.fd, buf, sizeof( buf ) );
    if ( rv < 0 && ( errno == EINTR || errno == EWOULDBLOCK || errno == EAGAIN ) )
    {
        return; //! not ready
    }

    if ( rv < 0 || errno == ECONNRESET )
    {
        msg_errno( "read() error" );
        conn.want_close = true;
        return;
    }

    if ( rv == 0 )
    {
        if ( conn.incoming.size() == 0 )
        {
            std::string message( "[client:" + std::to_string( conn.port ) +
                                 "] - closed" );
            msg( message.c_str() );
        }
        else
        {
            msg( "unexpected EOF" );
        }
        conn.want_close = true;
        return;
    }

    //! got new data
    buf_append( conn.incoming, buf, static_cast<size_t>(rv) );

    //! handel requests line...
    while ( try_one_request( conn ) )
    {
    };

    if ( conn.outgoing.size() > 0 )
    {
        conn.want_read = false;
        conn.want_write = true;
        return handle_write( conn );
    }
}

int main ()
{
    signal( SIGPIPE, SIG_IGN );

    int fd = socket( AF_INET, SOCK_STREAM, 0 );
    if ( fd < 0 )
    {
        die( "Error creating socket" );
    }

    //! Set socket options for reuse after restart
    int reuseSockVal = 1;
    setsockopt( fd, SOL_SOCKET, SO_REUSEADDR, &reuseSockVal, sizeof( reuseSockVal ) );

    //! Bind the socket to a port
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl( 0 );
    addr.sin_port = htons( 1234 );

    int rv = bind( fd, reinterpret_cast<sockaddr *>(&addr), sizeof( addr ) );
    if ( rv < 0 )
    {
        die( "Error binding socket" );
    }

    //! set listen nonblocking
    fd_set_nb( fd );

    //! Listen for incoming connections
    rv = listen( fd, SOMAXCONN );
    if ( rv < 0 )
    {
        die( "listen() error" );
    }

    //! a map of all client connections, keyed by fd
    std::vector<Conn *> fd2conn;
    std::unordered_map<int, std::unique_ptr<Conn> > fd2connMap;

    //! the event loop
    std::vector<pollfd> poll_args;
    while ( true )
    {
        poll_args.clear();

        poll_args.emplace_back( pollfd{fd, POLLIN, 0} );

        for ( const auto & [key, conn]: fd2connMap )
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

            poll_args.emplace_back( pfd );
        }

        //! wait for readiness
        int rv = poll( poll_args.data(), static_cast<nfds_t>(poll_args.size()), 0 );
        if ( rv < 0 && errno == EINTR )
        {
            continue; //! not an error
        }

        if ( rv < 0 )
        {
            die( "poll() error" );
        }

        //! handle the listening socket
        if ( poll_args[0].revents )
        {
            if ( std::unique_ptr<Conn> conn = std::move( handle_accept( fd ) ) )
            {
                //! put it into the map
                assert( !fd2connMap[conn->fd] );
                fd2connMap[conn->fd] = std::move( conn );
            }
        }

        for ( size_t i = 1; i < poll_args.size(); i++ )
        {
            short ready = poll_args[i].revents;
            if ( ready == 0 )
            {
                continue;
            }

            Conn & conn = *fd2connMap[poll_args[i].fd];

            if ( ready & POLLIN )
            {
                assert( conn.want_read );
                handle_read( conn );
            }

            if ( ready & POLLOUT )
            {
                assert( conn.want_write );
                handle_write( conn );
            }

            if ( ready & POLLERR || conn.want_close )
            {
                close( conn.fd );
                fd2connMap.erase( conn.fd );
            }
        }
    }
    return 0;
}
