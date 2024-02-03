#include "ADQAPI.h"
#include "mock_control_unit.h"

/* A static instance of the mocked ADQAPI. The user can decide whether to
   instantiate the object themselves or to use the CreateADQControlUnit()
   interface. We only support one control unit for now. */
static MockControlUnit mock_control_unit;
static bool has_added_digitizers = false;

/* Mockup control functions */
void MockControlUnit::AddDigitizer(enum ADQProductID_Enum pid,
                                   const ADQConstantParameters &constant)
{
    m_info_list.push_back({pid});
    m_digitizers.push_back(std::make_unique<MockDigitizer>(constant));
}

/* Mocked functions */
int MockControlUnit::SetupDevice(int index)
{
    /* 0-indexed to follow the convention of the ADQAPI. */
    if ((index < 0) || (index >= static_cast<int>(m_digitizers.size())))
        return 0;

    return m_digitizers[index]->SetupDevice();
}

int MockControlUnit::ListDevices(ADQInfoListEntry **list, unsigned int *nof_devices)
{
    if ((list == NULL) || (nof_devices == NULL))
        return 0;

    *list = &m_info_list[0];
    *nof_devices = static_cast<unsigned int>(m_info_list.size());
    return 1;
}

int MockControlUnit::OpenDeviceInterface(int index)
{
    /* 0-indexed to follow the convention of the ADQAPI. */
    if ((index < 0) || (index >= static_cast<int>(m_digitizers.size())))
        return 0;

    /* If the index targets an entry in the vector, we can consider it 'opened'. */
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    return 1;
}

int MockControlUnit::EnableErrorTrace(unsigned int level, const char *directory)
{
    /* Don't do anything for now. */
    (void)level;
    (void)directory;
    return 1;
}

int MockControlUnit::StartDataAcquisition(int adq_num)
{
    /* -1 to follow the convention of the ADQAPI. */
    if ((adq_num == 0) || (adq_num > static_cast<int>(m_digitizers.size())))
        return ADQ_EINVAL;
    return m_digitizers[adq_num - 1]->StartDataAcquisition();
}

int MockControlUnit::StopDataAcquisition(int adq_num)
{
    /* -1 to follow the convention of the ADQAPI. */
    if ((adq_num == 0) || (adq_num > static_cast<int>(m_digitizers.size())))
        return ADQ_EINVAL;
    return m_digitizers[adq_num - 1]->StopDataAcquisition();
}

int64_t MockControlUnit::WaitForRecordBuffer(int adq_num, int *channel, void **buffer, int timeout,
                                             ADQDataReadoutStatus *status)
{
    /* -1 to follow the convention of the ADQAPI. */
    if ((adq_num == 0) || (adq_num > static_cast<int>(m_digitizers.size())))
        return ADQ_EINVAL;
    return m_digitizers[adq_num - 1]->WaitForRecordBuffer(channel, buffer, timeout, status);
}

int MockControlUnit::ReturnRecordBuffer(int adq_num, int channel, void *buffer)
{
    /* -1 to follow the convention of the ADQAPI. */
    if ((adq_num == 0) || (adq_num > static_cast<int>(m_digitizers.size())))
        return ADQ_EINVAL;
    return m_digitizers[adq_num - 1]->ReturnRecordBuffer(channel, buffer);
}

int MockControlUnit::GetParameters(int adq_num, enum ADQParameterId id, void *const parameters)
{
    /* -1 to follow the convention of the ADQAPI. */
    if ((adq_num == 0) || (adq_num > static_cast<int>(m_digitizers.size())))
        return ADQ_EINVAL;
    return m_digitizers[adq_num - 1]->GetParameters(id, parameters);
}

int MockControlUnit::GetStatus(int adq_num, enum ADQStatusId id, void *const status)
{
    /* -1 to follow the convention of the ADQAPI. */
    if ((adq_num == 0) || (adq_num > static_cast<int>(m_digitizers.size())))
        return ADQ_EINVAL;
    return m_digitizers[adq_num - 1]->GetStatus(id, status);
}

int MockControlUnit::InitializeParametersString(int adq_num, enum ADQParameterId id, char *const string,
                                           size_t length, int format)
{
    /* -1 to follow the convention of the ADQAPI. */
    if ((adq_num == 0) || (adq_num > static_cast<int>(m_digitizers.size())))
        return ADQ_EINVAL;
    return m_digitizers[adq_num - 1]->InitializeParametersString(id, string, length, format);
}

int MockControlUnit::SetParametersString(int adq_num, const char *const string, size_t length)
{
    /* -1 to follow the convention of the ADQAPI. */
    if ((adq_num == 0) || (adq_num > static_cast<int>(m_digitizers.size())))
        return ADQ_EINVAL;
    return m_digitizers[adq_num - 1]->SetParametersString(string, length);
}

int MockControlUnit::GetParametersString(int adq_num, enum ADQParameterId id, char *const string, size_t length, int format)
{
    /* -1 to follow the convention of the ADQAPI. */
    if ((adq_num == 0) || (adq_num > static_cast<int>(m_digitizers.size())))
        return ADQ_EINVAL;
    return m_digitizers[adq_num - 1]->GetParametersString(id, string, length, format);
}

int MockControlUnit::ValidateParametersString(int adq_num,  const char *const string, size_t length)
{
    /* -1 to follow the convention of the ADQAPI. */
    if ((adq_num == 0) || (adq_num > static_cast<int>(m_digitizers.size())))
        return ADQ_EINVAL;
    return m_digitizers[adq_num - 1]->ValidateParametersString(string, length);
}

int MockControlUnit::SmTransaction(int adq_num, uint16_t cmd, void *wr_buf, size_t wr_buf_len,
                              void *rd_buf, size_t rd_buf_len)
{
    /* -1 to follow the convention of the ADQAPI. */
    if ((adq_num == 0) || (adq_num > static_cast<int>(m_digitizers.size())))
        return ADQ_EINVAL;
    return m_digitizers[adq_num - 1]->SmTransaction(cmd, wr_buf, wr_buf_len, rd_buf, rd_buf_len);
}

int MockControlUnit::SmTransactionImmediate(int adq_num, uint16_t cmd, void *wr_buf, size_t wr_buf_len,
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
        mock_control_unit.AddDigitizer(PID_ADQ32, {"SPD-SIM01",
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

        mock_control_unit.AddDigitizer(PID_ADQ36, {"SPD-SIM02",
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

    return &mock_control_unit;
}

void DeleteADQControlUnit(void *adq_cu)
{
    (void)adq_cu;
    return;
}

/* ADQAPI_ interface */
uint32_t ADQAPI_GetRevision()
{
    return 0xdeadbeefu;
}

const char *ADQAPI_GetRevisionString()
{
    return SIGSCAPE_REVISION;
}

int ADQAPI_ValidateVersion(int major, int minor)
{
    (void)major;
    (void)minor;
    return 0;
}

/* ADQControlUnit_ interface */
int ADQControlUnit_SetupDevice(void *adq_cu, int adq_num)
{
    if (adq_cu == NULL)
        return ADQ_EINVAL;
    return static_cast<MockControlUnit *>(adq_cu)->SetupDevice(adq_num);
}

int ADQControlUnit_ListDevices(void *adq_cu, ADQInfoListEntry **list, unsigned int *nof_devices)
{
    if (adq_cu == NULL)
        return ADQ_EINVAL;
    return static_cast<MockControlUnit *>(adq_cu)->ListDevices(list, nof_devices);
}

int ADQControlUnit_OpenDeviceInterface(void *adq_cu, int index)
{
    if (adq_cu == NULL)
        return ADQ_EINVAL;
    return static_cast<MockControlUnit *>(adq_cu)->OpenDeviceInterface(index);
}

int ADQControlUnit_EnableErrorTrace(void *adq_cu, unsigned int level, const char *directory)
{
    if (adq_cu == NULL)
        return ADQ_EINVAL;
    return static_cast<MockControlUnit *>(adq_cu)->EnableErrorTrace(level, directory);
}

/* ADQ_ interface */
int ADQ_StartDataAcquisition(void *adq_cu, int adq_num)
{
    if (adq_cu == NULL)
        return ADQ_EINVAL;
    return static_cast<MockControlUnit *>(adq_cu)->StartDataAcquisition(adq_num);
}

int ADQ_StopDataAcquisition(void *adq_cu, int adq_num)
{
    if (adq_cu == NULL)
        return ADQ_EINVAL;
    return static_cast<MockControlUnit *>(adq_cu)->StopDataAcquisition(adq_num);
}

int64_t ADQ_WaitForRecordBuffer(void *adq_cu, int adq_num, int *channel, void **buffer, int timeout,
                                ADQDataReadoutStatus *status)
{
    if (adq_cu == NULL)
        return ADQ_EINVAL;
    return static_cast<MockControlUnit *>(adq_cu)->WaitForRecordBuffer(
        adq_num, channel, buffer, timeout, status);
}

int ADQ_ReturnRecordBuffer(void *adq_cu, int adq_num, int channel, void *buffer)
{
    if (adq_cu == NULL)
        return ADQ_EINVAL;
    return static_cast<MockControlUnit *>(adq_cu)->ReturnRecordBuffer(adq_num, channel, buffer);
}

int ADQ_GetParameters(void *adq_cu, int adq_num, enum ADQParameterId id, void *const parameters)
{
    if (adq_cu == NULL)
        return ADQ_EINVAL;
    return static_cast<MockControlUnit *>(adq_cu)->GetParameters(adq_num, id, parameters);
}

int ADQ_GetStatus(void *adq_cu, int adq_num, enum ADQStatusId id, void *const status)
{
    if (adq_cu == NULL)
        return ADQ_EINVAL;
    return static_cast<MockControlUnit *>(adq_cu)->GetStatus(adq_num, id, status);
}

int ADQ_InitializeParametersString(void *adq_cu, int adq_num, enum ADQParameterId id, char *const string, size_t length, int format)
{
    if (adq_cu == NULL)
        return ADQ_EINVAL;
    return static_cast<MockControlUnit *>(adq_cu)->InitializeParametersString(adq_num, id, string, length, format);
}

int ADQ_SetParametersString(void *adq_cu, int adq_num, const char *const string, size_t length)
{
    if (adq_cu == NULL)
        return ADQ_EINVAL;
    return static_cast<MockControlUnit *>(adq_cu)->SetParametersString(adq_num, string, length);
}

int ADQ_GetParametersString(void *adq_cu, int adq_num, enum ADQParameterId id, char *const string, size_t length, int format)
{
    if (adq_cu == NULL)
        return ADQ_EINVAL;
    return static_cast<MockControlUnit *>(adq_cu)->GetParametersString(adq_num, id, string, length, format);
}

int ADQ_ValidateParametersString(void *adq_cu, int adq_num,  const char *const string, size_t length)
{
    if (adq_cu == NULL)
        return ADQ_EINVAL;
    return static_cast<MockControlUnit *>(adq_cu)->ValidateParametersString(adq_num, string, length);
}

int ADQ_SmTransaction(void *adq_cu, int adq_num, uint16_t cmd, void *wr_buf, size_t wr_buf_len,
                      void *rd_buf, size_t rd_buf_len)
{
    if (adq_cu == NULL)
        return ADQ_EINVAL;
    return static_cast<MockControlUnit *>(adq_cu)->SmTransaction(adq_num, cmd, wr_buf, wr_buf_len, rd_buf, rd_buf_len);
}

int ADQ_SmTransactionImmediate(void *adq_cu, int adq_num, uint16_t cmd, void *wr_buf,
                               size_t wr_buf_len, void *rd_buf, size_t rd_buf_len)
{
    if (adq_cu == NULL)
        return ADQ_EINVAL;
    return static_cast<MockControlUnit *>(adq_cu)->SmTransactionImmediate(adq_num, cmd, wr_buf, wr_buf_len, rd_buf, rd_buf_len);
}
