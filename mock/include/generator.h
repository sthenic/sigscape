#pragma once

#include "smart_buffer_thread.h"
#include "error.h"
#include "ADQAPI.h"
#include "nlohmann/json.hpp"

#include <random>

/* This is an abstract base class wrapping a `SmartBufferThread`. Here we
   implement the basic properties of a generator. Classes inheriting from this
   one must implement a `Generate` method to create ADQGen4Records that go into
   one or more output channels. This method gets called with a rate specified by
   the return value of `GetTriggerFrequency`. */

enum class GeneratorMessageId
{
    SET_TOP_PARAMETERS,
    GET_TOP_PARAMETERS,
    SET_CLOCK_SYSTEM_PARAMETERS,
    GET_CLOCK_SYSTEM_PARAMETERS,
    ENABLE,
    DISABLE,
};

struct GeneratorMessage
{
    GeneratorMessage() = default;

    GeneratorMessage(GeneratorMessageId id)
        : id{id}
        , json()
        , result{}
    {};

    GeneratorMessage(GeneratorMessageId id, const nlohmann::json &json)
        : id{id}
        , json(json)
        , result{}
    {};

    GeneratorMessageId id;
    nlohmann::json json;
    int result;
};

class Generator
    : public SmartBufferThread<ADQGen4Record, GeneratorMessage, true>
{
public:
    Generator(size_t nof_channels)
        : SmartBufferThread(nof_channels)
        , m_random_generator{
            static_cast<uint32_t>(std::chrono::steady_clock::now().time_since_epoch().count())}
        , m_distribution{0, 0.1}
        , m_record_number{0}
        , m_timestamp{0}
        , m_enabled{false}
    {}

protected:
    /* TODO: 25ps steps for now. */
    static constexpr double TIME_UNIT = 25e-12;

    std::default_random_engine m_random_generator;
    std::normal_distribution<double> m_distribution;
    uint32_t m_record_number;
    uint64_t m_timestamp;
    bool m_enabled;

    /* Provide a default implementation of header generation. We only set the
       members we have information about on this level. */
    virtual void SeedHeader(ADQGen4RecordHeader *header)
    {
        *header = {};
        header->record_number = m_record_number;
        header->record_start = static_cast<int64_t>(m_distribution(m_random_generator) * 1000);
        header->time_unit = TIME_UNIT;
        header->timestamp = m_timestamp;
        header->sampling_period = static_cast<uint64_t>(
            std::round(1.0 / (TIME_UNIT * GetSamplingFrequency())));

        if (!(m_record_number % 50))
            header->misc |= 0x1u;
        if (!(m_record_number % 30))
            header->misc |= 0x2u;

        header->timestamp_synchronization_counter = static_cast<uint16_t>(m_record_number / 100);
    }

private:
    /* Required overrides of a derived class. */
    virtual void Generate() = 0;
    virtual double GetTriggerFrequency() = 0;
    virtual double GetSamplingFrequency() = 0;
    virtual double GetNoise() = 0;
    virtual int GetParameters(GeneratorMessageId id, nlohmann::json &json) = 0;
    virtual int SetParameters(GeneratorMessageId id, const nlohmann::json &json) = 0;

    void MainLoop() override
    {
        m_thread_exit_code = SCAPE_EOK;

        for (;;)
        {
            const auto start = std::chrono::high_resolution_clock::now();
            ProcessMessages();

            if (m_enabled)
            {
                /* Run the generator. */
                Generate();

                /* Update bookkeeping variables owned by this class. */
                m_record_number++;
                m_timestamp += static_cast<uint64_t>(
                    std::round(1.0 / (TIME_UNIT * GetTriggerFrequency())));
            }

            /* If the generator is not enabled, we sleep for 100 ms to still be
               able to process messages. Otherwise, we sleep for a the rest of
               the trigger period. */
            const auto delta = (std::chrono::high_resolution_clock::now() - start).count();
            const int wait_us = static_cast<int>(1000000.0 / std::max(0.5, GetTriggerFrequency()));
            const int remainder_us = m_enabled
                                         ? std::max(wait_us - static_cast<int>(delta / 1e3), 0)
                                         : 100000;

            /* We implement the sleep using the stop event to be able to
               immediately react to the event being set. */
            if (m_should_stop.wait_for(std::chrono::microseconds(remainder_us)) == std::future_status::ready)
                break;
        }
    }

    void ProcessMessages()
    {
        StampedMessage message;
        while (SCAPE_EOK == _WaitForMessage(message, 0))
        {
            switch (message.contents.id)
            {
            case GeneratorMessageId::SET_TOP_PARAMETERS:
            case GeneratorMessageId::SET_CLOCK_SYSTEM_PARAMETERS:
            {
                /* Reset the random distribution when parameters are applied successfully. */
                message.contents.result = SetParameters(message.contents.id, message.contents.json);
                if (message.contents.result == SCAPE_EOK)
                    m_distribution = std::normal_distribution<double>(0, GetNoise());
                break;
            }

            case GeneratorMessageId::GET_TOP_PARAMETERS:
            case GeneratorMessageId::GET_CLOCK_SYSTEM_PARAMETERS:
            {
                message.contents.result = GetParameters(message.contents.id, message.contents.json);
                break;
            }

            case GeneratorMessageId::ENABLE:
            {
                m_enabled = true;
                m_timestamp = 0;
                m_record_number = 0;
                message.contents.result = SCAPE_EOK;
                break;
            }

            case GeneratorMessageId::DISABLE:
            {
                m_enabled = false;
                message.contents.result = SCAPE_EOK;
                break;
            }

            default:
            {
                fprintf(
                    stderr, "Generator: Unknown message id '%d'.\n", static_cast<int>(message.id));
                message.contents.result = SCAPE_EINVAL;
                break;
            }
            }

            /* Always send a response. */
            _PushMessage(std::move(message));
        }
    }
};
