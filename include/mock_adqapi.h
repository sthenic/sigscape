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
    int SetupDevice(int adq_num);

    int StartDataAcquisition(int adq_num);
    int StopDataAcquisition(int adq_num);
    int64_t WaitForRecordBuffer(int adq_num, int *channel, void **buffer, int timeout,
                                struct ADQDataReadoutStatus *status);
    int ReturnRecordBuffer(int adq_num, int channel, void *buffer);

    int GetParameters(int adq_num, enum ADQParameterId id, void *const parameters);

    int InitializeParametersString(int adq_num, enum ADQParameterId id, char *const string, size_t length, int format);
    int SetParametersString(int adq_num, const char *const string, size_t length);
    int GetParametersString(int adq_num, enum ADQParameterId id, char *const string, size_t length, int format);
    int ValidateParametersString(int adq_num,  const char *const string, size_t length);

private:
    std::vector<std::unique_ptr<Generator>> m_generators;
    static const std::string DEFAULT_PARAMETERS;
    static const std::string DEFAULT_CLOCK_SYSTEM_PARAMETERS;

    template<typename T>
    int ParseLine(int line_idx, const std::string &str, std::vector<T> &values);
};

/* The ADQControlUnit_* functions we're mocking. */
int ADQControlUnit_SetupDevice(void *adq_cu, int adq_num);

/* The ADQ_* functions we're mocking. */
int ADQ_StartDataAcquisition(void *adq_cu, int adq_num);
int ADQ_StopDataAcquisition(void *adq_cu, int adq_num);
int64_t ADQ_WaitForRecordBuffer(void *adq_cu, int adq_num, int *channel, void **buffer, int timeout,
                                struct ADQDataReadoutStatus *status);
int ADQ_ReturnRecordBuffer(void *adq_cu, int adq_num, int channel, void *buffer);

int ADQ_GetParameters(void *adq_cu, int adq_num, enum ADQParameterId id, void *const parameters);

int ADQ_InitializeParametersString(void *adq_cu, int adq_num, enum ADQParameterId id, char *const string, size_t length, int format);
int ADQ_SetParametersString(void *adq_cu, int adq_num, const char *const string, size_t length);
int ADQ_GetParametersString(void *adq_cu, int adq_num, enum ADQParameterId id, char *const string, size_t length, int format);
int ADQ_ValidateParametersString(void *adq_cu, int adq_num,  const char *const string, size_t length);

#endif
