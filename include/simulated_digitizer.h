/*
 * An abstract base class that defines the interface for a digitizer.
 */

#ifndef SIMULATED_DIGITIZER_H_QBRXXD
#define SIMULATED_DIGITIZER_H_QBRXXD

#include "digitizer.h"
#include "data_processing.h"
#include "simulator.h"

class SimulatedDigitizer : public Digitizer
{
public:
    SimulatedDigitizer();

    /* Initialize the digitizer. */
    int Initialize();

    int WaitForProcessedRecord(int channel, std::shared_ptr<ProcessedRecord> &record);

private:
    void MainLoop() override;

    std::vector<std::shared_ptr<DataAcquisitionSimulator>> m_simulator;
    std::vector<std::unique_ptr<DataProcessing>> m_data_processing;

    /* The digitizer's parameter set. */

    int HandleMessage(const struct DigitizerMessage &msg);
    // int DoAcquisition();
};

#endif
