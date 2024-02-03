#pragma once

#include "ADQAPI.h"
#include "sine_generator.h"
#include "mock_system_manager.h"
#include <vector>

class MockDigitizer
{
public:
    MockDigitizer(const ADQConstantParameters &constant);

    /* Mocked functions. */
    int SetupDevice();

    int StartDataAcquisition();
    int StopDataAcquisition();
    int64_t WaitForRecordBuffer(int *channel, void **buffer, int timeout,
                                ADQDataReadoutStatus *status);
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
    ADQConstantParameters m_constant;
    ADQAnalogFrontendParameters m_afe;
    ADQClockSystemParameters m_clock_system;
    ADQDataTransferParameters m_transfer;
    ADQDramStatus m_dram_status;
    ADQOverflowStatus m_overflow_status;
    std::vector<SineGenerator> m_generators;
    MockSystemManager m_sysman;
    std::string m_top_parameters;
    std::string m_clock_system_parameters;
};
