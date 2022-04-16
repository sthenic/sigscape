#include "mock_adqapi.h"

/* Mockup control functions */
void MockAdqApi::AddDigitizer(const std::string &serial_number, int nof_channels)
{
    m_digitizers.push_back(std::make_unique<MockDigitizer>(serial_number, nof_channels));
}

/* Mocked functions */
int MockAdqApi::SetupDevice(int adq_num)
{
    /* -1 to follow the convention of the ADQAPI. */
    if ((adq_num == 0) || (adq_num > static_cast<int>(m_digitizers.size())))
        return ADQ_EINVAL;
    return m_digitizers[adq_num - 1]->SetupDevice();
}

int MockAdqApi::StartDataAcquisition(int adq_num)
{
    /* -1 to follow the convention of the ADQAPI. */
    if ((adq_num == 0) || (adq_num > static_cast<int>(m_digitizers.size())))
        return ADQ_EINVAL;
    return m_digitizers[adq_num - 1]->StartDataAcquisition();
}

int MockAdqApi::StopDataAcquisition(int adq_num)
{
    /* -1 to follow the convention of the ADQAPI. */
    if ((adq_num == 0) || (adq_num > static_cast<int>(m_digitizers.size())))
        return ADQ_EINVAL;
    return m_digitizers[adq_num - 1]->StopDataAcquisition();
}

int64_t MockAdqApi::WaitForRecordBuffer(int adq_num, int *channel, void **buffer, int timeout,
                                        struct ADQDataReadoutStatus *status)
{
    /* -1 to follow the convention of the ADQAPI. */
    if ((adq_num == 0) || (adq_num > static_cast<int>(m_digitizers.size())))
        return ADQ_EINVAL;
    return m_digitizers[adq_num - 1]->WaitForRecordBuffer(channel, buffer, timeout, status);
}

int MockAdqApi::ReturnRecordBuffer(int adq_num, int channel, void *buffer)
{
    /* -1 to follow the convention of the ADQAPI. */
    if ((adq_num == 0) || (adq_num > static_cast<int>(m_digitizers.size())))
        return ADQ_EINVAL;
    return m_digitizers[adq_num - 1]->ReturnRecordBuffer(channel, buffer);
}

int MockAdqApi::GetParameters(int adq_num, enum ADQParameterId id, void *const parameters)
{
    /* -1 to follow the convention of the ADQAPI. */
    if ((adq_num == 0) || (adq_num > static_cast<int>(m_digitizers.size())))
        return ADQ_EINVAL;
    return m_digitizers[adq_num - 1]->GetParameters(id, parameters);
}

int MockAdqApi::InitializeParametersString(int adq_num, enum ADQParameterId id, char *const string,
                                           size_t length, int format)
{
    /* -1 to follow the convention of the ADQAPI. */
    if ((adq_num == 0) || (adq_num > static_cast<int>(m_digitizers.size())))
        return ADQ_EINVAL;
    return m_digitizers[adq_num - 1]->InitializeParametersString(id, string, length, format);
}

int MockAdqApi::SetParametersString(int adq_num, const char *const string, size_t length)
{
    /* -1 to follow the convention of the ADQAPI. */
    if ((adq_num == 0) || (adq_num > static_cast<int>(m_digitizers.size())))
        return ADQ_EINVAL;
    return m_digitizers[adq_num - 1]->SetParametersString(string, length);
}

int MockAdqApi::GetParametersString(int adq_num, enum ADQParameterId id, char *const string, size_t length, int format)
{
    /* -1 to follow the convention of the ADQAPI. */
    if ((adq_num == 0) || (adq_num > static_cast<int>(m_digitizers.size())))
        return ADQ_EINVAL;
    return m_digitizers[adq_num - 1]->GetParametersString(id, string, length, format);
}

int MockAdqApi::ValidateParametersString(int adq_num,  const char *const string, size_t length)
{
    /* -1 to follow the convention of the ADQAPI. */
    if ((adq_num == 0) || (adq_num > static_cast<int>(m_digitizers.size())))
        return ADQ_EINVAL;
    return m_digitizers[adq_num - 1]->ValidateParametersString(string, length);
}

/* ADQControlUnit_ interface */
int ADQControlUnit_SetupDevice(void *adq_cu, int adq_num)
{
    if (adq_cu == NULL)
        return ADQ_EINVAL;
    return static_cast<MockAdqApi *>(adq_cu)->SetupDevice(adq_num);
}

/* ADQ_ interface */
int ADQ_StartDataAcquisition(void *adq_cu, int adq_num)
{
    if (adq_cu == NULL)
        return ADQ_EINVAL;
    return static_cast<MockAdqApi *>(adq_cu)->StartDataAcquisition(adq_num);
}

int ADQ_StopDataAcquisition(void *adq_cu, int adq_num)
{
    if (adq_cu == NULL)
        return ADQ_EINVAL;
    return static_cast<MockAdqApi *>(adq_cu)->StopDataAcquisition(adq_num);
}

int64_t ADQ_WaitForRecordBuffer(void *adq_cu, int adq_num, int *channel, void **buffer, int timeout,
                                struct ADQDataReadoutStatus *status)
{
    if (adq_cu == NULL)
        return ADQ_EINVAL;
    return static_cast<MockAdqApi *>(adq_cu)->WaitForRecordBuffer(adq_num, channel, buffer, timeout,
                                                                  status);
}

int ADQ_ReturnRecordBuffer(void *adq_cu, int adq_num, int channel, void *buffer)
{
    if (adq_cu == NULL)
        return ADQ_EINVAL;
    return static_cast<MockAdqApi *>(adq_cu)->ReturnRecordBuffer(adq_num, channel, buffer);
}

int ADQ_GetParameters(void *adq_cu, int adq_num, enum ADQParameterId id, void *const parameters)
{
    if (adq_cu == NULL)
        return ADQ_EINVAL;
    return static_cast<MockAdqApi *>(adq_cu)->GetParameters(adq_num, id, parameters);
}

int ADQ_InitializeParametersString(void *adq_cu, int adq_num, enum ADQParameterId id, char *const string, size_t length, int format)
{
    if (adq_cu == NULL)
        return ADQ_EINVAL;
    return static_cast<MockAdqApi *>(adq_cu)->InitializeParametersString(adq_num, id, string, length, format);
}

int ADQ_SetParametersString(void *adq_cu, int adq_num, const char *const string, size_t length)
{
    if (adq_cu == NULL)
        return ADQ_EINVAL;
    return static_cast<MockAdqApi *>(adq_cu)->SetParametersString(adq_num, string, length);
}

int ADQ_GetParametersString(void *adq_cu, int adq_num, enum ADQParameterId id, char *const string, size_t length, int format)
{
    if (adq_cu == NULL)
        return ADQ_EINVAL;
    return static_cast<MockAdqApi *>(adq_cu)->GetParametersString(adq_num, id, string, length, format);
}

int ADQ_ValidateParametersString(void *adq_cu, int adq_num,  const char *const string, size_t length)
{
    if (adq_cu == NULL)
        return ADQ_EINVAL;
    return static_cast<MockAdqApi *>(adq_cu)->ValidateParametersString(adq_num, string, length);
}

