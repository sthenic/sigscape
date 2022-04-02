/*
 * An abstract base class that defines the interface for a digitizer.
 */

#ifndef SIMULATED_DIGITIZER_H_QBRXXD
#define SIMULATED_DIGITIZER_H_QBRXXD

#include "digitizer.h"
#include "simulator.h"

class SimulatedDigitizer : public Digitizer
{
public:
    SimulatedDigitizer();

    int Initialize();

private:
    void MainLoop() override;

    std::vector<std::shared_ptr<DataAcquisitionSimulator>> m_simulator;

    /* FIXME: ProcessMessages() */
    int HandleMessage(const struct DigitizerMessage &msg);

    void ProcessWatcherMessages(
        const std::unique_ptr<FileWatcher> &watcher,
        const std::unique_ptr<ThreadSafeQueue<std::shared_ptr<std::string>>> &queue);
};

#endif
