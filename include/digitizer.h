/*
 * An abstract base class that defines the interface of a digitizer.
 */

#ifndef DIGITIZER_H_TMDVZV
#define DIGITIZER_H_TMDVZV

#include "message_thread.h"
#include "data_types.h"
#include "ADQAPI.h"

#include <array>

enum DigitizerMessageId
{
    MESSAGE_ID_SETUP_STARTING,
    MESSAGE_ID_SETUP_OK,
    MESSAGE_ID_SETUP_FAILED,
    MESSAGE_ID_PARAMETER_SET_UPDATED_FROM_FILE,
    MESSAGE_ID_PARAMETER_SET_UPDATED_FROM_GUI, /* FIXME: Maybe we keep it read only in the GUI. */
    MESSAGE_ID_START_ACQUISITION,
    MESSAGE_ID_STOP_ACQUISITION,
    MESSAGE_ID_NEW_STATE
};

enum DigitizerState
{
    STATE_NOT_ENUMERATED,
    STATE_INITIALIZATION,
    STATE_CONFIGURATION,
    STATE_ACQUISITION
};

struct DigitizerMessage
{
    enum DigitizerMessageId id;
    int status;
    void *data;
};

class Digitizer : public MessageThread<Digitizer, struct DigitizerMessage>
{
public:
    Digitizer()
        : MessageThread()
        , m_state(STATE_NOT_ENUMERATED)
    {
    }

    /* FIXME: Needed? */
    ~Digitizer()
    {
        Stop();
    }

    /* Delete copy constructors. */
    Digitizer(const Digitizer &) = delete;
    Digitizer &operator=(const Digitizer &) = delete;

protected:
    /* TODO: Perhaps write a file watcher w/ C++17's std::filesystem:
       https://solarianprogrammer.com/2019/01/13/cpp-17-filesystem-write-file-watcher-monitor/ */

    enum DigitizerState m_state;
};

#endif
