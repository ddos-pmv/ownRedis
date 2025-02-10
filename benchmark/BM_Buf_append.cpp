#include <benchmark/benchmark.h>
#include <deque>
#include <vector>

static void deque_append(std::deque<uint8_t> &buf, const uint8_t * data, size_t len)
{
    buf.insert(buf.end(), data, data + len);
}

static void vector_append(std::vector<uint8_t> &buf, const uint8_t * data, ssize_t len)
{
    buf.insert(buf.end(), data, data + len);
}

static void deque_consume(std::deque<uint8_t> &buf, size_t n)
{
    buf.erase(buf.begin(), buf.begin() + n);
}

static void vector_consume(std::vector<uint8_t> &buf, size_t n)
{
    buf.erase(buf.begin(), buf.begin() + n);
}


static void BM_DequeAppend(benchmark::State &state)
{
    std::deque<uint8_t> buf;
    std::vector<uint8_t> data(state.range(0), 42);
    for ( auto _: state )
    {
        deque_append(buf, data.data(), data.size());
        deque_consume(buf, state.range(0) / 2);
        buf.clear();
    }
}


static void BM_VectorAppend(benchmark::State &state)
{
    std::vector<uint8_t> buf;
    std::vector<uint8_t> data(state.range(0), 42);
    for ( auto _: state )
    {
        vector_append(buf, data.data(), data.size());
        vector_consume(buf, state.range(0) / 2);
        buf.clear();
    }
}

BENCHMARK(BM_DequeAppend)->Range(1, 1 << 16);
BENCHMARK(BM_VectorAppend)->Range(1, 1 << 16);

BENCHMARK_MAIN();
