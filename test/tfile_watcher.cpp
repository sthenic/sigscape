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
    LONGS_EQUAL(SCAPE_EOK, watcher.Start());

    /* Expect a clean message queue. */
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    FileWatcherMessage message;
    LONGS_EQUAL(SCAPE_EAGAIN, watcher.WaitForMessage(message, 0));

    /* Create the file. */
    std::ofstream ofs(PATH, std::ios::out);
    LONGS_EQUAL(SCAPE_EOK, watcher.WaitForMessage(message, 300));
    LONGS_EQUAL(FileWatcherMessageId::FILE_CREATED, message.id);
    STRCMP_EQUAL("", message.contents->c_str());

    /* Write some contents (the flush triggers the timestamp change). */
    ofs << "Hello!";
    ofs.flush();
    LONGS_EQUAL(SCAPE_EOK, watcher.WaitForMessage(message, 300));
    LONGS_EQUAL(FileWatcherMessageId::FILE_UPDATED, message.id);
    STRCMP_EQUAL("Hello!", message.contents->c_str());

    ofs << "\nAdding some more text.";
    ofs.flush();
    LONGS_EQUAL(SCAPE_EOK, watcher.WaitForMessage(message, 300));
    LONGS_EQUAL(FileWatcherMessageId::FILE_UPDATED, message.id);
    STRCMP_EQUAL("Hello!\nAdding some more text.", message.contents->c_str());

    /* Close the file, expecting a clean message queue. */
    ofs.close();
    LONGS_EQUAL(SCAPE_EAGAIN, watcher.WaitForMessage(message, 0));

    /* Remove the file. */
    std::filesystem::remove(PATH);
    LONGS_EQUAL(SCAPE_EOK, watcher.WaitForMessage(message, 300));
    LONGS_EQUAL(FileWatcherMessageId::FILE_DELETED, message.id);
    STRCMP_EQUAL("", message.contents->c_str());

    LONGS_EQUAL(SCAPE_EOK, watcher.Stop());
}

TEST(FileWatcher, WriteToFile)
{
    const std::string PATH = "./foo.txt";
    std::filesystem::remove(PATH);

    FileWatcher watcher(PATH);
    LONGS_EQUAL(SCAPE_EOK, watcher.Start());

    /* Expect a clean message queue. */
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    FileWatcherMessage message;
    LONGS_EQUAL(SCAPE_EAGAIN, watcher.WaitForMessage(message, 0));

    static const char CONTENTS0[] = "Initial contents for 'foo.txt'.\n";
    LONGS_EQUAL(SCAPE_EOK, watcher.PushMessage({FileWatcherMessageId::UPDATE_FILE,
                                               std::make_shared<std::string>(CONTENTS0)}));

    LONGS_EQUAL(SCAPE_EOK, watcher.WaitForMessage(message, 600));
    LONGS_EQUAL(FileWatcherMessageId::FILE_CREATED, message.id);
    STRCMP_EQUAL(CONTENTS0, message.contents->c_str());

    static const char CONTENTS1[] = "Replace everything with this!\n";
    LONGS_EQUAL(SCAPE_EOK, watcher.PushMessage({FileWatcherMessageId::UPDATE_FILE,
                                               std::make_shared<std::string>(CONTENTS1)}));

    LONGS_EQUAL(SCAPE_EOK, watcher.WaitForMessage(message, 600));
    LONGS_EQUAL(FileWatcherMessageId::FILE_UPDATED, message.id);
    STRCMP_EQUAL(CONTENTS1, message.contents->c_str());

    LONGS_EQUAL(SCAPE_EOK, watcher.Stop());
}
