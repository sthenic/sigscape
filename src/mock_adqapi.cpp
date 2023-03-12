#include "mock_adqapi.h"

/* A static instance of the mocked ADQAPI. The user can decide whether to
   instantiate the object themselves or to use the CreateADQControlUnit()
   interface. We only supports one control unit for now. */
static MockAdqApi mock_adqapi;
static bool has_added_digitizers = false;

/* Mockup control functions */
void MockAdqApi::AddDigitizer(enum ADQProductID_Enum pid,
                              const struct ADQConstantParameters &constant)
{
    m_info_list.push_back({pid});
    m_digitizers.push_back(std::make_unique<MockDigitizer>(constant));
}

/* Mocked functions */
int MockAdqApi::SetupDevice(int index)
{
    /* 0-indexed to follow the convention of the ADQAPI. */
    if ((index < 0) || (index >= static_cast<int>(m_digitizers.size())))
        return 0;

    return m_digitizers[index]->SetupDevice();
}

int MockAdqApi::ListDevices(struct ADQInfoListEntry **list, unsigned int *nof_devices)
{
    if ((list == NULL) || (nof_devices == NULL))
        return 0;

    *list = &m_info_list[0];
    *nof_devices = static_cast<unsigned int>(m_info_list.size());
    return 1;
}

int MockAdqApi::OpenDeviceInterface(int index)
{
    /* 0-indexed to follow the convention of the ADQAPI. */
    if ((index < 0) || (index >= static_cast<int>(m_digitizers.size())))
        return 0;

    /* If the index targets an entry in the vector, we can consider it 'opened'. */
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    return 1;
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

int MockAdqApi::GetStatus(int adq_num, enum ADQStatusId id, void *const status)
{
    /* -1 to follow the convention of the ADQAPI. */
    if ((adq_num == 0) || (adq_num > static_cast<int>(m_digitizers.size())))
        return ADQ_EINVAL;
    return m_digitizers[adq_num - 1]->GetStatus(id, status);
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

int MockAdqApi::SmTransaction(int adq_num, uint16_t cmd, void *wr_buf, size_t wr_buf_len,
                              void *rd_buf, size_t rd_buf_len)
{
    /* -1 to follow the convention of the ADQAPI. */
    if ((adq_num == 0) || (adq_num > static_cast<int>(m_digitizers.size())))
        return ADQ_EINVAL;
    return m_digitizers[adq_num - 1]->SmTransaction(cmd, wr_buf, wr_buf_len, rd_buf, rd_buf_len);
}

int MockAdqApi::SmTransactionImmediate(int adq_num, uint16_t cmd, void *wr_buf, size_t wr_buf_len,
                                       void *rd_buf, size_t rd_buf_len)
{
    /* -1 to follow the convention of the ADQAPI. */
    if ((adq_num == 0) || (adq_num > static_cast<int>(m_digitizers.size())))
        return ADQ_EINVAL;
    return m_digitizers[adq_num - 1]->SmTransactionImmediate(cmd, wr_buf, wr_buf_len, rd_buf, rd_buf_len);
}

void *CreateADQControlUnit()
{
    /* The first time the control unit is created, we add a few simulated
       digitizers to the system. */
    if (!has_added_digitizers)
    {
        mock_adqapi.AddDigitizer(PID_ADQ32, {"SPD-SIM01",
                                             "ADQ32",
                                             "-SG2G5-BW1G0",
                                             {
                                                 ADQ_FIRMWARE_TYPE_FWDAQ,
                                                 "1CH-FWDAQ",
                                                 "2023.1.3",
                                                 "STANDARD",
                                                 "400-000-XYZ",
                                             },
                                             {
                                                 ADQ_COMMUNICATION_INTERFACE_PCIE,
                                                 3,
                                                 8,
                                             },
                                             {
                                                 {"A", 2, {2500.0}, 65536},
                                             }});

        mock_adqapi.AddDigitizer(PID_ADQ36, {"SPD-SIM02",
                                             "ADQ36",
                                             "-SG2G5-BW2G5",
                                             {
                                                 ADQ_FIRMWARE_TYPE_FWDAQ,
                                                 "2CH-FWDAQ",
                                                 "2023.1.2",
                                                 "STANDARD",
                                                 "400-001-XYZ",
                                             },
                                             {
                                                 ADQ_COMMUNICATION_INTERFACE_PCIE,
                                                 2,
                                                 4,
                                             },
                                             {
                                                 {"A", 1, {2500.0}, 65536},
                                                 {"B", 2, {1000.0}, 65536},
                                             }});
        has_added_digitizers = true;
    }

    return &mock_adqapi;
}

void DeleteADQControlUnit(void *adq_cu)
{
    /* FIXME: any cleanup? */
    (void)adq_cu;
    return;
}

/* ADQControlUnit_ interface */
int ADQControlUnit_SetupDevice(void *adq_cu, int adq_num)
{
    if (adq_cu == NULL)
        return ADQ_EINVAL;
    return static_cast<MockAdqApi *>(adq_cu)->SetupDevice(adq_num);
}

int ADQControlUnit_ListDevices(void *adq_cu, struct ADQInfoListEntry **list, unsigned int *nof_devices)
{
    if (adq_cu == NULL)
        return ADQ_EINVAL;
    return static_cast<MockAdqApi *>(adq_cu)->ListDevices(list, nof_devices);
}

int ADQControlUnit_OpenDeviceInterface(void *adq_cu, int index)
{
    if (adq_cu == NULL)
        return ADQ_EINVAL;
    return static_cast<MockAdqApi *>(adq_cu)->OpenDeviceInterface(index);
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

int ADQ_GetStatus(void *adq_cu, int adq_num, enum ADQStatusId id, void *const status)
{
    if (adq_cu == NULL)
        return ADQ_EINVAL;
    return static_cast<MockAdqApi *>(adq_cu)->GetStatus(adq_num, id, status);
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

int ADQ_SmTransaction(void *adq_cu, int adq_num, uint16_t cmd, void *wr_buf, size_t wr_buf_len,
                      void *rd_buf, size_t rd_buf_len)
{
    if (adq_cu == NULL)
        return ADQ_EINVAL;
    return static_cast<MockAdqApi *>(adq_cu)->SmTransaction(adq_num, cmd, wr_buf, wr_buf_len, rd_buf, rd_buf_len);
}

int ADQ_SmTransactionImmediate(void *adq_cu, int adq_num, uint16_t cmd, void *wr_buf,
                               size_t wr_buf_len, void *rd_buf, size_t rd_buf_len)
{
    if (adq_cu == NULL)
        return ADQ_EINVAL;
    return static_cast<MockAdqApi *>(adq_cu)->SmTransactionImmediate(adq_num, cmd, wr_buf, wr_buf_len, rd_buf, rd_buf_len);
}
