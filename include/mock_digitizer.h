#ifndef MOCK_DIGITIZER_H_STIYJN
#define MOCK_DIGITIZER_H_STIYJN

#include "generator.h"
#include "mock_system_manager.h"
#include "mock_adqapi_definitions.h"
#include <vector>

class MockDigitizer
{
public:
    MockDigitizer(const std::string &serial_number,
                  const struct ADQConstantParametersFirmware &firmware,
                  const std::vector<double> &input_range, int nof_channels,
                  const std::vector<int> &nof_adc_cores);

    /* FIXME: Mockup control functions. */

    /* Mocked functions. */
    int SetupDevice();

    int StartDataAcquisition();
    int StopDataAcquisition();
    int64_t WaitForRecordBuffer(int *channel, void **buffer, int timeout,
                                struct ADQDataReadoutStatus *status);
    int ReturnRecordBuffer(int channel, void *buffer);

    int GetParameters(enum ADQParameterId id, void *const parameters);
    int GetStatus(enum ADQStatusId id, void *const status);

    int InitializeParametersString(enum ADQParameterId id, char *const string, size_t length, int format);
    int SetParametersString(const char *const string, size_t length);
    int GetParametersString(enum ADQParameterId id, char *const string, size_t length, int format);
    int ValidateParametersString(const char *const string, size_t length);

    int SmTransaction(uint16_t cmd, void *wr_buf, size_t wr_buf_len, void *rd_buf, size_t rd_buf_len);
    int SmTransactionImmediate(uint16_t cmd, void *wr_buf, size_t wr_buf_len, void *rd_buf, size_t rd_buf_len);
private:
    struct ADQConstantParameters m_constant;
    struct ADQAnalogFrontendParameters m_afe;
    struct ADQDramStatus m_dram_status;
    struct ADQOverflowStatus m_overflow_status;
    std::vector<std::unique_ptr<Generator>> m_generators;
    std::unique_ptr<MockSystemManager> m_sysman;
    static const std::string DEFAULT_TOP_PARAMETERS;
    static const std::string DEFAULT_CLOCK_SYSTEM_PARAMETERS;
    std::string m_top_parameters;
    std::string m_clock_system_parameters;

    template<typename T>
    int ParseLine(int line_idx, const std::string &str, std::vector<T> &values);
};

#endif
