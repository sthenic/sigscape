#include <thread>
#include <chrono>
#include <set>
#include "message_thread.h"

#include "CppUTest/TestHarness.h"

enum MessageId
{
    MESSAGE_ID_IS_STARTED_OK,
    MESSAGE_ID_FAILED_TO_START,
    MESSAGE_ID_NEW_DATA,
    MESSAGE_ID_RETURN_DATA
};

struct Message
{
    enum MessageId id;
    int code;
    int *data;
};

class TestMessageThread : public MessageThread<TestMessageThread, struct Message>
{
public:
    TestMessageThread()
        : m_start_code(0)
        , m_nof_messages_to_generate(10)
    {}

    ~TestMessageThread()
    {
        FreeGeneratedData();
    }

    void Initialize(int start_code, int nof_messages_to_generate)
    {
        m_start_code = start_code;
        m_nof_messages_to_generate = nof_messages_to_generate;
    }

    void MainLoop()
    {
        struct Message start_msg;
        start_msg.id = (m_start_code != 0) ? MESSAGE_ID_FAILED_TO_START : MESSAGE_ID_IS_STARTED_OK;
        start_msg.code = m_start_code;
        start_msg.data = NULL;

        int result = _PushMessage(start_msg);
        if (result != SCAPE_EOK)
        {
            printf("Failed to write the start message, result %d.\n", result);
            m_thread_exit_code = result;
            return;
        }

        m_generated_data.clear();
        m_returned_data.clear();

        for (;;)
        {
            /* Continue on 'ok' and 'timeout'. */
            struct Message read_msg;
            result = _WaitForMessage(read_msg, 10);
            if ((result != SCAPE_EOK) && (result != SCAPE_EAGAIN))
            {
                m_thread_exit_code = result;
                break;
            }

            if (result == SCAPE_EOK)
            {
                result = HandleMessage(read_msg);
                if (result != SCAPE_EOK)
                {
                    printf("Failed to handle message w/ id %d, result %d.\n", read_msg.id, result);
                    m_thread_exit_code = result;
                    return;
                }
            }

            if (m_nof_messages_to_generate > 0)
            {
                int *data = new int[16];
                for (int i = 0; i < 16; ++i)
                    data[i] = i;

                m_generated_data.insert(data);

                struct Message write_msg;
                write_msg.id = MESSAGE_ID_NEW_DATA;
                write_msg.code = 0;
                write_msg.data = data;

                result = _PushMessage(write_msg);
                if (result != SCAPE_EOK)
                {
                    printf("Failed to write new data message, result %d.\n", result);
                    m_thread_exit_code = result;
                    return;
                }

                m_nof_messages_to_generate -= 1;
            }

            if (m_should_stop.wait_for(std::chrono::milliseconds(100)) == std::future_status::ready)
                break;
        }
    }

    int VerifyComplete()
    {
        /* Check that we got the expected number of replies and that the content
           matched. */

        auto generated_data = m_generated_data;
        for (const auto &data : m_generated_data)
        {
            if (m_returned_data.count(data) == 0)
            {
                printf("%p has not been returned.\n", data);
                return SCAPE_EINTERNAL;
            }

            generated_data.erase(data);
            m_returned_data.erase(data);
        }

        if (generated_data.size() > 0)
        {
            printf("%zu elements have not been returned.\n", generated_data.size());
            return SCAPE_EINTERNAL;
        }

        if (m_returned_data.size() > 0)
        {
            printf("%zu returned elements remain.\n", m_returned_data.size());
            return SCAPE_EINTERNAL;
        }

        FreeGeneratedData();
        return SCAPE_EOK;
    }

private:
    std::set<int *> m_generated_data;
    std::set<int *> m_returned_data;
    int m_start_code;
    int m_nof_messages_to_generate;

    int HandleMessage(const struct Message &msg)
    {
        /* We only expect to receive messages of this type. */
        if (msg.id != MESSAGE_ID_RETURN_DATA)
            return SCAPE_EINTERNAL;

        m_returned_data.insert(msg.data);
        return SCAPE_EOK;
    }

    void FreeGeneratedData()
    {
        for (auto &v : m_generated_data)
            delete[] v;
        m_generated_data.clear();
    }
};


TEST_GROUP(MessageThread)
{
    TestMessageThread thread;

    void setup()
    {
    }

    void teardown()
    {
        thread.Stop();
    }
};

TEST(MessageThread, FailedStart)
{
    constexpr int CODE = -88;
    thread.Initialize(CODE, 10);
    LONGS_EQUAL(SCAPE_EOK, thread.Start());

    struct Message msg{};
    LONGS_EQUAL(SCAPE_EOK, thread.WaitForMessage(msg, 1000));
    LONGS_EQUAL(MESSAGE_ID_FAILED_TO_START, msg.id);
    LONGS_EQUAL(CODE, msg.code);
    CHECK(msg.data == NULL);
    thread.Stop();
}

TEST(MessageThread, RevolvingMessages)
{
    constexpr int NOF_MESSAGES = 10;
    thread.Initialize(0, NOF_MESSAGES);
    LONGS_EQUAL(SCAPE_EOK, thread.Start());

    /* Expect the 'is started ok' message. */
    struct Message msg;
    LONGS_EQUAL(SCAPE_EOK, thread.WaitForMessage(msg, 500));
    LONGS_EQUAL(MESSAGE_ID_IS_STARTED_OK, msg.id);
    LONGS_EQUAL(SCAPE_EOK, msg.code);

    for (int i = 0; i < NOF_MESSAGES; ++i)
    {
        LONGS_EQUAL(SCAPE_EOK, thread.WaitForMessage(msg, 500));
        LONGS_EQUAL(MESSAGE_ID_NEW_DATA, msg.id);
        LONGS_EQUAL(SCAPE_EOK, msg.code);

        msg.id = MESSAGE_ID_RETURN_DATA;
        LONGS_EQUAL(SCAPE_EOK, thread.PushMessage(msg));
    }

    /* Expect a timeout. */
    LONGS_EQUAL(SCAPE_EAGAIN, thread.WaitForMessage(msg, 500));
    thread.Stop();

    LONGS_EQUAL(SCAPE_EOK, thread.VerifyComplete());
}
