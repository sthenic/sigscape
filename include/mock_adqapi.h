#ifndef MOCK_ADQAPI_H_5FQFYM
#define MOCK_ADQAPI_H_5FQFYM

#include "mock_adqapi_definitions.h"
#include "mock_digitizer.h"
#include <vector>

/* This is a tiny mockup of the ADQAPI to allow us to run without the real
   ADQAPI and a digitizer connected to the host system. */

class MockAdqApi
{
public:
    MockAdqApi() = default;

    /* Mockup control functions. */
    void AddDigitizer(const std::string &serial_number,
                      const struct ADQConstantParametersFirmware &firmware,
                      const std::vector<double> &input_range, int nof_channels,
                      enum ADQProductID_Enum pid);

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

    int InitializeParametersString(int adq_num, enum ADQParameterId id, char *const string, size_t length, int format);
    int SetParametersString(int adq_num, const char *const string, size_t length);
    int GetParametersString(int adq_num, enum ADQParameterId id, char *const string, size_t length, int format);
    int ValidateParametersString(int adq_num,  const char *const string, size_t length);

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

int ADQ_InitializeParametersString(void *adq_cu, int adq_num, enum ADQParameterId id, char *const string, size_t length, int format);
int ADQ_SetParametersString(void *adq_cu, int adq_num, const char *const string, size_t length);
int ADQ_GetParametersString(void *adq_cu, int adq_num, enum ADQParameterId id, char *const string, size_t length, int format);
int ADQ_ValidateParametersString(void *adq_cu, int adq_num,  const char *const string, size_t length);

#endif
