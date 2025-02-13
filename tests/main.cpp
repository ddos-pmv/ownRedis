#include <gtest/gtest.h>
#include <string>
#include "../src/handlers.h"
#include <vector>


// Demonstrate some basic assertions.
TEST(HelloTest, BasicAssertions)
{
    std::vector<uint8_t> in = {5, 'h', 'e', 'l', 'l', 'o'};

    Conn conn;
    conn.incoming = in;
    ASSERT_EQ(handle_one_line(&conn), true);
}

int main()
{
    ::testing::InitGoogleTest();
    return RUN_ALL_TESTS();
}
