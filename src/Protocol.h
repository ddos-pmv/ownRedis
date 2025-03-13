#ifndef RESP_H
#define RESP_H

#include <vector>
#include <string>
#include <unistd.h>
#include <iostream>

class Protocol {
public:
    static int32_t parse_request(const uint8_t * src, std::vector<std::string> &dist, size_t len);

private:
    static bool read_u32(const uint8_t *&src, const uint8_t * srcEnd, uint32_t * dist);

    static bool read_str(const uint8_t *&src, const uint8_t * end, uint32_t strLen, std::string &dist);

    static void msg(const char * msg);

    static size_t k_max_msg;
    static size_t k_max_args;
};
#endif //RESP_H
