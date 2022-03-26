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
    ~SimulatedDigitizer();

    /* Initialize the digitizer. */
    int Initialize();

    int Start() override;
    int Stop() override;

    /* Consider moving all this unto */
    int WaitForProcessedRecord(struct ProcessedRecord *&record)
    {
        return m_data_read_queue.Read(record, 0);
    }

private:
    void MainLoop() override;

    DataAcquisitionSimulator m_simulator;
    DataProcessing m_data_processing;

    /* FIXME: Need arrays for each channel. */
    ThreadSafeQueue<struct ProcessedRecord *> m_data_read_queue;

    /* The digitizer's parameter set. */

    int HandleMessage(const struct DigitizerMessage &msg);
    int DoAcquisition();
};

#endif
