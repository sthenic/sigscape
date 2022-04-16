/*
 * An abstract base class that defines the interface for a digitizer.
 */

#ifndef SIMULATED_DIGITIZER_H_QBRXXD
#define SIMULATED_DIGITIZER_H_QBRXXD

#include "digitizer.h"
#include "generator.h"

class SimulatedDigitizer : public Digitizer
{
public:
    SimulatedDigitizer(int index);

private:
    MockAdqApi m_adqapi;

    void MainLoop() override;
    void ProcessMessages();

    int SetParameters();

    void InitializeFileWatchers(const struct ADQConstantParameters &constant);
};

#endif
