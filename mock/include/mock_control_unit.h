#pragma once

#include "mock_digitizer.h"
#include <vector>
#include <cinttypes>

/* This is a tiny mockup of the ADQAPI to allow us to run without the real
   ADQAPI and a digitizer connected to the host system. */

class MockControlUnit
{
public:
    MockControlUnit() = default;

    /* Mockup control functions. */
    void AddDigitizer(enum ADQProductID_Enum pid, const struct ADQConstantParameters &constant);

    /* Mocked functions. */
    int SetupDevice(int index);
    int ListDevices(struct ADQInfoListEntry **list, unsigned int *nof_devices);
    int OpenDeviceInterface(int index);
    int EnableErrorTrace(unsigned int level, const char *directory);

    int StartDataAcquisition(int adq_num);
    int StopDataAcquisition(int adq_num);
    int64_t WaitForRecordBuffer(int adq_num, int *channel, void **buffer, int timeout,
                                struct ADQDataReadoutStatus *status);
    int ReturnRecordBuffer(int adq_num, int channel, void *buffer);

    int GetParameters(int adq_num, enum ADQParameterId id, void *const parameters);
    int GetStatus(int adq_num, enum ADQStatusId id, void *const status);

    int InitializeParametersString(int adq_num, enum ADQParameterId id, char *const string,
                                   size_t length, int format);
    int SetParametersString(int adq_num, const char *const string, size_t length);
    int GetParametersString(int adq_num, enum ADQParameterId id, char *const string, size_t length,
                            int format);
    int ValidateParametersString(int adq_num, const char *const string, size_t length);

    int SmTransaction(int adq_num, uint16_t cmd, void *wr_buf, size_t wr_buf_len, void *rd_buf,
                      size_t rd_buf_len);

    int SmTransactionImmediate(int adq_num, uint16_t cmd, void *wr_buf, size_t wr_buf_len,
                               void *rd_buf, size_t rd_buf_len);

  private:
    std::vector<std::unique_ptr<MockDigitizer>> m_digitizers;
    std::vector<ADQInfoListEntry> m_info_list;
};
