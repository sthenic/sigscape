#include "embedded_python.h"
#include "embedded_python_thread.h"
#include "log.h"

#include <filesystem>
#include <type_traits>

EmbeddedPythonThread::~EmbeddedPythonThread()
{
    Stop();
}

bool EmbeddedPythonThread::IsInitialized() const
{
    return EmbeddedPython::IsInitialized();
}

bool EmbeddedPythonThread::IsPyadqCompatible()
{
    return SCAPE_EOK == PushMessageWaitForResponse(
        {EmbeddedPythonMessageId::IS_PYADQ_COMPATIBLE}, [](auto &r) { return r.result; });
}

int EmbeddedPythonThread::AddToPath(const std::string &directory)
{
    return PushMessageWaitForResponse(
        {EmbeddedPythonMessageId::ADD_TO_PATH, directory}, [](auto &r) { return r.result; });
}

bool EmbeddedPythonThread::HasMain(const std::filesystem::path &path)
{
    return SCAPE_EOK == PushMessageWaitForResponse(
        {EmbeddedPythonMessageId::HAS_MAIN, path.string()}, [](auto &r) { return r.result; });
}

int EmbeddedPythonThread::CallMain(
    const std::string &module, void *handle, int index, std::string &out)
{
    return PushMessageWaitForResponse(
        {EmbeddedPythonMessageId::CALL_MAIN, module, handle, index}, [&](auto &r) {
            out = std::move(r.str);
            return r.result;
        });
}

void EmbeddedPythonThread::MainLoop()
{
    /* If no Python session is initialized then this thread has no purpose. */
    if (!EmbeddedPython::IsInitialized())
    {
        m_thread_exit_code = SCAPE_EINTERNAL;
        return;
    }

    Log::log->trace("Started embedded Python session manager.");
    for (;;)
    {
        HandleMessages();
        if (m_should_stop.wait_for(std::chrono::seconds(0)) == std::future_status::ready)
            break;
    }


    m_thread_exit_code = SCAPE_EOK;
}

void EmbeddedPythonThread::HandleMessages()
{
    StampedMessage message;
    while (SCAPE_EOK == _WaitForMessage(message, 250))
    {
        try
        {
            switch (message.contents.id)
            {
            case EmbeddedPythonMessageId::IS_PYADQ_COMPATIBLE:
                IsPyadqCompatible(message);
                break;

            case EmbeddedPythonMessageId::ADD_TO_PATH:
                AddToPath(message);
                break;

            case EmbeddedPythonMessageId::HAS_MAIN:
                HasMain(message);
                break;

            case EmbeddedPythonMessageId::CALL_MAIN:
                CallMain(message);
                break;

            default:
                break;
            }
        }
        catch (const EmbeddedPythonException &e)
        {
            /* TODO: Log error? */
            StampedMessage response{message.id};
            response.contents.result = SCAPE_EINTERNAL;
            response.contents.str = std::move(e.what());
            _PushMessage(std::move(response));
        }
    }
}

void EmbeddedPythonThread::IsPyadqCompatible(const StampedMessage &message)
{
    /* The caller expects a response w/ a matching id. */
    StampedMessage response{message.id};
    response.contents.result = EmbeddedPython::IsPyadqCompatible() ? SCAPE_EOK : SCAPE_EAGAIN;
    _PushMessage(response);
}

void EmbeddedPythonThread::AddToPath(const StampedMessage &message)
{
    /* The caller expects a response w/ a matching id. */
    StampedMessage response{message.id};
    EmbeddedPython::AddToPath(message.contents.str);
    Log::log->trace("Embedded Python added path '{}'.", message.contents.str);
    _PushMessage(response);
}

void EmbeddedPythonThread::HasMain(const StampedMessage &message)
{
    /* The caller expects a response w/ a matching id. */
    StampedMessage response{message.id};
    Log::log->trace("Embedded Python checking path '{}'.", message.contents.str);
    if (EmbeddedPython::HasMain(message.contents.str))
    {
        response.contents.result = SCAPE_EOK;
        Log::log->trace("Found callable main() in '{}'.", message.contents.str);
    }
    else
    {
        response.contents.result = SCAPE_EAGAIN;
        Log::log->error(
            "Embedded Python failed to find callable main() in '{}'.", message.contents.str);
    }
    _PushMessage(response);
}


void EmbeddedPythonThread::CallMain(const StampedMessage &message)
{
    /* The caller expects a response w/ a matching id. */
    StampedMessage response{message.id};
    EmbeddedPython::CallMain(
        message.contents.str, message.contents.handle, message.contents.index,
        response.contents.str);
    Log::log->trace("Embedded Python called main() in module {}.", message.contents.str);
    _PushMessage(response);
}

