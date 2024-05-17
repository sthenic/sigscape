#include "message_channels.h"
#include <thread>
#include <set>
#include <map>
#include "CppUTest/TestHarness.h"

struct TestMessage
{
    int code;
};

class TestChannels : public MessageChannels<TestMessage>
{
public:
    /* Make public for testing purposes. */
    int _WaitForMessage(TestMessage &message, int timeout, uint32_t &id)
    {
        return MessageChannels<TestMessage>::_WaitForMessage(message, timeout, id);
    }
};

TEST_GROUP(MessageChannels)
{
    TestChannels channels;
};

TEST(MessageChannels, StampedMessages)
{
    static constexpr int NOF_THREADS = 4;

    auto PushMessages = [&]()
    {
        uint32_t id;
        for (int i = 0; i < 1000; ++i)
            channels.PushMessage({i}, id);
    };

    std::vector<std::thread> pool;
    for (int i = 0; i < NOF_THREADS; ++i)
        pool.emplace_back(PushMessages);

    for (auto &t : pool)
        t.join();

    /* Check that we only see each id exactly once (but never the value zero)
       and that each code is observed NOF_THREADS times. */
    std::set<uint32_t> ids;
    std::map<int, int> codes;

    uint32_t id{};
    TestMessage message{};

    while (SCAPE_EOK == channels._WaitForMessage(message, 0, id))
    {
        LONGS_EQUAL(0, ids.count(id));
        ids.insert(id);
        codes[message.code] += 1;
    }

    LONGS_EQUAL(0, ids.count(0));
    LONGS_EQUAL(1, std::all_of(codes.cbegin(), codes.cend(), [&](const auto &kv) {
                    const auto &[code, count] = kv;
                    return count == NOF_THREADS;
                }));
}
