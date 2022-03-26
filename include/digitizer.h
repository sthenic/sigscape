/*
 * An abstract base class that defines the interface for a digitizer.
 */

#ifndef DIGITIZER_H_TMDVZV
#define DIGITIZER_H_TMDVZV

#include "message_thread.h"
#include "data_types.h"
#include "ADQAPI.h"

#include <mutex>
#include <algorithm>

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
    /* FIXME: Delete default constructor? */
    Digitizer()
        : m_state(STATE_NOT_ENUMERATED)
    {}

    ~Digitizer() = default;

    /* Delete copy constructors. Needed? */
    Digitizer(const Digitizer &) = delete;
    Digitizer &operator=(const Digitizer &) = delete;

    /* FIXME: Make this into an abstract class. */

protected:
    /* TODO: Perhaps write a file watcher w/ C++17's std::filesystem:
       https://solarianprogrammer.com/2019/01/13/cpp-17-filesystem-write-file-watcher-monitor/ */
    enum DigitizerState m_state;
};

#endif
