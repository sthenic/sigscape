#include "directory_watcher.h"

#include <fstream>
#include "CppUTest/TestHarness.h"

TEST_GROUP(DirectoryWatcher)
{
};

TEST(DirectoryWatcher, WatchDirectory)
{
    const std::filesystem::path PATH = "./tmp";
    const std::filesystem::path FILE1 = PATH / "file1.txt";
    const std::filesystem::path FILE2 = PATH / "file2.txt";
    std::filesystem::remove_all(PATH);

    DirectoryWatcher watcher{PATH};
    LONGS_EQUAL(SCAPE_EOK, watcher.Start());

    /* Expect a clean message queue. */
    DirectoryWatcherMessage message;
    LONGS_EQUAL(SCAPE_EAGAIN, watcher.WaitForMessage(message, 1100));

    /* Create the directory and a few files. */
    std::filesystem::create_directory(PATH);
    std::ofstream ofs1{FILE1, std::ios::out};

    LONGS_EQUAL(SCAPE_EOK, watcher.WaitForMessage(message, 1100));
    LONGS_EQUAL(DirectoryWatcherMessageId::FILE_CREATED, message.id);
    STRCMP_EQUAL(FILE1.c_str(), message.path.string().c_str());

    std::ofstream ofs2{FILE2, std::ios::out};
    LONGS_EQUAL(SCAPE_EOK, watcher.WaitForMessage(message, 1100));
    LONGS_EQUAL(DirectoryWatcherMessageId::FILE_CREATED, message.id);
    STRCMP_EQUAL(FILE2.c_str(), message.path.string().c_str());

    /* Write something to the files (the flush triggers the timestamp change). */
    ofs1 << "Hello World!";
    ofs1.flush();
    LONGS_EQUAL(SCAPE_EOK, watcher.WaitForMessage(message, 1100));
    LONGS_EQUAL(DirectoryWatcherMessageId::FILE_UPDATED, message.id);
    STRCMP_EQUAL(FILE1.c_str(), message.path.string().c_str());

    ofs2 << "Amazing file tracking";
    ofs2.flush();
    LONGS_EQUAL(SCAPE_EOK, watcher.WaitForMessage(message, 1100));
    LONGS_EQUAL(DirectoryWatcherMessageId::FILE_UPDATED, message.id);
    STRCMP_EQUAL(FILE2.c_str(), message.path.string().c_str());

    /* Close the files, expecting a clean message queue. */
    ofs1.close();
    ofs2.close();
    LONGS_EQUAL(SCAPE_EAGAIN, watcher.WaitForMessage(message, 0));

    /* Remove a single file, then the entire directory. */
    std::filesystem::remove(FILE1);
    LONGS_EQUAL(SCAPE_EOK, watcher.WaitForMessage(message, 1100));
    LONGS_EQUAL(DirectoryWatcherMessageId::FILE_DELETED, message.id);
    STRCMP_EQUAL(FILE1.c_str(), message.path.string().c_str());

    std::filesystem::remove_all(PATH);
    LONGS_EQUAL(SCAPE_EOK, watcher.WaitForMessage(message, 1100));
    LONGS_EQUAL(DirectoryWatcherMessageId::FILE_DELETED, message.id);
    STRCMP_EQUAL(FILE2.c_str(), message.path.string().c_str());

    /* Expect a clean message queue, then exit. */
    LONGS_EQUAL(SCAPE_EAGAIN, watcher.WaitForMessage(message, 1100));
    LONGS_EQUAL(SCAPE_EOK, watcher.Stop());
}

TEST(DirectoryWatcher, DontWatchFile)
{
    const std::filesystem::path FILE = "./tmp";
    std::filesystem::remove_all(FILE);

    DirectoryWatcher watcher{FILE};
    LONGS_EQUAL(SCAPE_EOK, watcher.Start());

    /* Expect a clean message queue. */
    DirectoryWatcherMessage message;
    LONGS_EQUAL(SCAPE_EAGAIN, watcher.WaitForMessage(message, 1100));

    /* Create a file with the target name and _not_ a directory. */
    std::ofstream ofs{FILE, std::ios::out};
    ofs << "Hello!";
    ofs.flush();

    /* Expect a clean message queue. */
    LONGS_EQUAL(SCAPE_EAGAIN, watcher.WaitForMessage(message, 1100));
    LONGS_EQUAL(SCAPE_EOK, watcher.Stop());
}

TEST(DirectoryWatcher, ExtensionFilter)
{
    const std::filesystem::path PATH = "./tmp";
    const std::filesystem::path FILE1 = PATH / "file1.txt";
    const std::filesystem::path FILE2 = PATH / "file2.py";
    std::filesystem::remove_all(PATH);

    DirectoryWatcher watcher{PATH, ".py"};
    LONGS_EQUAL(SCAPE_EOK, watcher.Start());

    /* Create the directory and a few files. */
    std::filesystem::create_directory(PATH);
    std::ofstream ofs1{FILE1, std::ios::out};

    /* Expect a clean message queue since .txt should be ignored. */
    DirectoryWatcherMessage message;
    LONGS_EQUAL(SCAPE_EAGAIN, watcher.WaitForMessage(message, 1100));

    std::ofstream ofs2{FILE2, std::ios::out};
    LONGS_EQUAL(SCAPE_EOK, watcher.WaitForMessage(message, 1100));
    LONGS_EQUAL(DirectoryWatcherMessageId::FILE_CREATED, message.id);
    STRCMP_EQUAL(FILE2.c_str(), message.path.string().c_str());

    LONGS_EQUAL(SCAPE_EOK, watcher.Stop());
}
