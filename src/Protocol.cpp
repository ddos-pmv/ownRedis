#include "Protocol.h"

size_t Protocol::k_max_msg = 32 << 20;
size_t Protocol::k_max_args = 200 * 1000;

int32_t Protocol::parse_request(const uint8_t * src, std::vector<std::string> &dist, size_t srcLen)
{
    const uint8_t * srcEnd = src + srcLen;
    uint32_t nstr = 0;
    if ( !read_u32(src, srcEnd, &nstr) )
    {
        msg("Failed to read nstr");
        return -1;
    }


    if ( nstr > k_max_args )
    {
        msg("Too many args");
        return -1; //! safety limit
    }


    dist.reserve(nstr);
    while ( dist.size() < nstr )
    {
        uint32_t len = 0;
        if ( !read_u32(src, srcEnd, &len) )
        {
            return -1;
        }
        dist.emplace_back();
        if ( !read_str(src, srcEnd, len, dist.back()) )
        {
            return -1;
        }
    }
    if ( src != srcEnd )
    {
        msg("src != srcEnd");
        return -1;
    }


    return 0;
}

bool Protocol::read_u32(const uint8_t *&src, const uint8_t * srcEnd, uint32_t * dst)
{
    if ( src + 4 > srcEnd )
    {
        msg("read_u32() error");
        return false;
    }

    std::memcpy(dst, src, 4);
    src += 4;
    return true;
}

bool Protocol::read_str(const uint8_t *&src, const uint8_t * srcEnd, uint32_t strLen, std::string &dist)
{
    if ( src + strLen > srcEnd )
    {
        msg("read_str() error");
        return false;
    }

    dist.assign(src, src + strLen);
    src += strLen;
    return true;
}

void Protocol::msg(const char * msg)
{
    std::cout << msg << '\n';
}


