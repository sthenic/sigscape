#ifndef MOCK_DIGITIZER_H_STIYJN
#define MOCK_DIGITIZER_H_STIYJN

#include "generator.h"
#include "mock_adqapi_definitions.h"
#include <vector>

class MockDigitizer
{
public:
    MockDigitizer(const std::string &serial_number, int nof_channels);

    /* FIXME: Mockup control functions. */

    /* Mocked functions. */
    int SetupDevice();

    int StartDataAcquisition();
    int StopDataAcquisition();
    int64_t WaitForRecordBuffer(int *channel, void **buffer, int timeout,
                                struct ADQDataReadoutStatus *status);
    int ReturnRecordBuffer(int channel, void *buffer);

    int GetParameters(enum ADQParameterId id, void *const parameters);

    int InitializeParametersString(enum ADQParameterId id, char *const string, size_t length, int format);
    int SetParametersString(const char *const string, size_t length);
    int GetParametersString(enum ADQParameterId id, char *const string, size_t length, int format);
    int ValidateParametersString( const char *const string, size_t length);

private:
    struct ADQConstantParameters m_constant;
    struct ADQAnalogFrontendParameters m_afe;
    std::vector<std::unique_ptr<Generator>> m_generators;
    static const std::string DEFAULT_TOP_PARAMETERS;
    static const std::string DEFAULT_CLOCK_SYSTEM_PARAMETERS;

    template<typename T>
    int ParseLine(int line_idx, const std::string &str, std::vector<T> &values);
};

#endif
