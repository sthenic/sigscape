#ifndef MOCK_ADQAPI_H_5FQFYM
#define MOCK_ADQAPI_H_5FQFYM

#include "mock/adqapi_definitions.h"
#include "mock/digitizer.h"
#include <vector>

/* This is a tiny mockup of the ADQAPI to allow us to run without the real
   ADQAPI and a digitizer connected to the host system. */

class MockAdqApi
{
public:
    MockAdqApi() = default;

    /* Mockup control functions. */
    void AddDigitizer(enum ADQProductID_Enum pid, const struct ADQConstantParameters &constant);

    /* Mocked functions. */
    int SetupDevice(int index);
    int ListDevices(struct ADQInfoListEntry **list, unsigned int *nof_devices);
    int OpenDeviceInterface(int index);

    int StartDataAcquisition(int adq_num);
    int StopDataAcquisition(int adq_num);
    int64_t WaitForRecordBuffer(int adq_num, int *channel, void **buffer, int timeout,
                                struct ADQDataReadoutStatus *status);
    int ReturnRecordBuffer(int adq_num, int channel, void *buffer);

    int GetParameters(int adq_num, enum ADQParameterId id, void *const parameters);
    int GetStatus(int adq_num, enum ADQStatusId id, void *const status);

    int InitializeParametersString(int adq_num, enum ADQParameterId id, char *const string, size_t length, int format);
    int SetParametersString(int adq_num, const char *const string, size_t length);
    int GetParametersString(int adq_num, enum ADQParameterId id, char *const string, size_t length, int format);
    int ValidateParametersString(int adq_num,  const char *const string, size_t length);

    int SmTransaction(int adq_num, uint16_t cmd, void *wr_buf, size_t wr_buf_len, void *rd_buf,
                      size_t rd_buf_len);

    int SmTransactionImmediate(int adq_num, uint16_t cmd, void *wr_buf, size_t wr_buf_len,
                               void *rd_buf, size_t rd_buf_len);

private:
    std::vector<std::unique_ptr<MockDigitizer>> m_digitizers;
    std::vector<ADQInfoListEntry> m_info_list;
};

void *CreateADQControlUnit();
void DeleteADQControlUnit(void *adq_cu);

/* The ADQControlUnit_* functions we're mocking. */
int ADQControlUnit_SetupDevice(void *adq_cu, int adq_num);
int ADQControlUnit_ListDevices(void *adq_cu, struct ADQInfoListEntry **list, unsigned int *nof_devices);
int ADQControlUnit_OpenDeviceInterface(void *adq_cu, int index);

/* The ADQ_* functions we're mocking. */
int ADQ_StartDataAcquisition(void *adq_cu, int adq_num);
int ADQ_StopDataAcquisition(void *adq_cu, int adq_num);
int64_t ADQ_WaitForRecordBuffer(void *adq_cu, int adq_num, int *channel, void **buffer, int timeout,
                                struct ADQDataReadoutStatus *status);
int ADQ_ReturnRecordBuffer(void *adq_cu, int adq_num, int channel, void *buffer);

int ADQ_GetParameters(void *adq_cu, int adq_num, enum ADQParameterId id, void *const parameters);
int ADQ_GetStatus(void *adq_cu, int adq_num, enum ADQStatusId id, void *const status);

int ADQ_InitializeParametersString(void *adq_cu, int adq_num, enum ADQParameterId id, char *const string, size_t length, int format);
int ADQ_SetParametersString(void *adq_cu, int adq_num, const char *const string, size_t length);
int ADQ_GetParametersString(void *adq_cu, int adq_num, enum ADQParameterId id, char *const string, size_t length, int format);
int ADQ_ValidateParametersString(void *adq_cu, int adq_num,  const char *const string, size_t length);

int ADQ_SmTransaction(void *adq_cu, int adq_num, uint16_t cmd, void *wr_buf, size_t wr_buf_len,
                      void *rd_buf, size_t rd_buf_len);

int ADQ_SmTransactionImmediate(void *adq_cu, int adq_num, uint16_t cmd, void *wr_buf,
                               size_t wr_buf_len, void *rd_buf, size_t rd_buf_len);

#endif
