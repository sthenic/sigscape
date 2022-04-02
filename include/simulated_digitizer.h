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
    std::vector<std::shared_ptr<DataAcquisitionSimulator>> m_simulator;

    static const std::string DEFAULT_PARAMETERS;
    static const std::string DEFAULT_CLOCK_SYSTEM_PARAMETERS;

    void MainLoop() override;
    void ProcessMessages();
    void ProcessWatcherMessages(
        const std::unique_ptr<FileWatcher> &watcher,
        const std::unique_ptr<ThreadSafeQueue<std::shared_ptr<std::string>>> &queue);

    template<typename T>
    int ParseLine(int line_idx, const std::string &str, std::vector<T> &values);
};

#endif
