#include "mock_adqapi.h"

void MockAdqApi::Initialize(const std::vector<Generator::Parameters> &parameters)
{
    m_generators.clear();
    for (const auto &p : parameters)
    {
        m_generators.push_back(std::make_unique<Generator>());
        m_generators.back()->Initialize(p);
    }
}

int MockAdqApi::StartDataAcquisition(int adq_num)
{
    (void)adq_num;
    for (const auto &g : m_generators)
        g->Start();

    return ADQ_EOK;
}

int MockAdqApi::StopDataAcquisition(int adq_num)
{
    (void)adq_num;
    for (const auto &g : m_generators)
        g->Stop();

    return ADQ_EOK;
}

int64_t MockAdqApi::WaitForRecordBuffer(int adq_num, int *channel, void **buffer, int timeout,
                                        struct ADQDataReadoutStatus *status)
{
    (void)adq_num;
    (void)status;

    if (buffer == NULL)
        return ADQ_EINVAL;
    if (*channel == -1)
        return ADQ_EUNSUPPORTED;
    if ((*channel < 0) || (static_cast<size_t>(*channel) >= m_generators.size()))
    {
        printf("Channel is %d.\n", *channel);
        return ADQ_EINVAL;
    }

    ADQGen4Record *lbuffer = NULL;
    int result = m_generators[*channel]->WaitForBuffer(lbuffer, timeout);
    *buffer = lbuffer;

    /* FIXME: Error code space etc.*/
    /* FIXME: Would be better if the generator return length. */
    if (result < 0)
        return result;
    else
        return lbuffer->header->record_length * sizeof(int16_t);
}

int MockAdqApi::ReturnRecordBuffer(int adq_num, int channel, void *buffer)
{
    (void)adq_num;

    if (buffer == NULL)
        return ADQ_EINVAL;
    if (channel == -1)
        return ADQ_EUNSUPPORTED;
    if ((channel < 0) || (static_cast<size_t>(channel) >= m_generators.size()))
        return ADQ_EINVAL;

    /* FIXME: Error space */
    return m_generators[channel]->ReturnBuffer(static_cast<ADQGen4Record *>(buffer));
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
