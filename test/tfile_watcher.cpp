#include "file_watcher.h"

#include <fstream>
#include "CppUTest/TestHarness.h"

TEST_GROUP(FileWatcher)
{
    void setup()
    {
    }

    void teardown()
    {
    }
};

TEST(FileWatcher, WatchFile)
{
    const std::string PATH = "./foo.txt";
    std::filesystem::remove(PATH);

    FileWatcher watcher(PATH);
    LONGS_EQUAL(ADQR_EOK, watcher.Start());

    FileWatcherMessage msg;
    LONGS_EQUAL(ADQR_EAGAIN, watcher.WaitForMessage(msg, 0));

    std::ofstream ofs(PATH, std::ios::out);
    LONGS_EQUAL(ADQR_EOK, watcher.WaitForMessage(msg, 300));
    LONGS_EQUAL(FileWatcherMessageId::FILE_CREATED, msg.id);
    STRCMP_EQUAL("", msg.contents->c_str());

    ofs << "Hello!";
    ofs.flush();
    LONGS_EQUAL(ADQR_EOK, watcher.WaitForMessage(msg, 300));
    LONGS_EQUAL(FileWatcherMessageId::FILE_UPDATED, msg.id);
    STRCMP_EQUAL("Hello!", msg.contents->c_str());

    ofs << "\nAdding some more text.";
    ofs.flush();
    LONGS_EQUAL(ADQR_EOK, watcher.WaitForMessage(msg, 300));
    LONGS_EQUAL(FileWatcherMessageId::FILE_UPDATED, msg.id);
    STRCMP_EQUAL("Hello!\nAdding some more text.", msg.contents->c_str());

    ofs.close();
    LONGS_EQUAL(ADQR_EAGAIN, watcher.WaitForMessage(msg, 0));

    std::filesystem::remove(PATH);
    LONGS_EQUAL(ADQR_EOK, watcher.WaitForMessage(msg, 300));
    LONGS_EQUAL(FileWatcherMessageId::FILE_DELETED, msg.id);
    STRCMP_EQUAL("", msg.contents->c_str());

    LONGS_EQUAL(ADQR_EOK, watcher.Stop());
}
