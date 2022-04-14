#ifndef MOCK_ADQAPI_H_5FQFYM
#define MOCK_ADQAPI_H_5FQFYM

#include "generator.h"
#include "mock_adqapi_definitions.h"
#include <vector>

/* This is a tiny mockup of the ADQAPI to allow us to run without the real
   ADQAPI and a digitizer connected to the host system. */
class MockAdqApi
{
public:
    MockAdqApi() = default;

    /* Mockup control functions. */
    void Initialize(const std::vector<Generator::Parameters> &parameters);

    /* Mocked functions. */
    int StartDataAcquisition(int adq_num);
    int StopDataAcquisition(int adq_num);
    int64_t WaitForRecordBuffer(int adq_num, int *channel, void **buffer, int timeout,
                                struct ADQDataReadoutStatus *status);
    int ReturnRecordBuffer(int adq_num, int channel, void *buffer);

private:
    std::vector<std::unique_ptr<Generator>> m_generators;
};

/* The ADQ_* functions. These are what we're after. */
int ADQ_StartDataAcquisition(void *adq_cu, int adq_num);
int ADQ_StopDataAcquisition(void *adq_cu, int adq_num);
int64_t ADQ_WaitForRecordBuffer(void *adq_cu, int adq_num, int *channel, void **buffer, int timeout,
                                struct ADQDataReadoutStatus *status);
int ADQ_ReturnRecordBuffer(void *adq_cu, int adq_num, int channel, void *buffer);

#endif
