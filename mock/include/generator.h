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
   `m_parameters.trigger_frequency`. Thus, the parameter struct `T` must at the
   very least contain this member. */

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

template<typename T, typename C>
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
        , m_top_parameters{}
        , m_clock_system_parameters{}
    {}

    ~Generator() override = default;

protected:
    /* TODO: 25ps steps for now. */
    static constexpr double TIME_UNIT = 25e-12;

    std::default_random_engine m_random_generator;
    std::normal_distribution<double> m_distribution;
    uint32_t m_record_number;
    uint32_t m_timestamp;
    bool m_enabled;
    T m_top_parameters;
    C m_clock_system_parameters;

    /* Provide a default implementation of header generator. */
    virtual void SeedHeader(ADQGen4RecordHeader *header)
    {
        *header = {};
        header->data_format = ADQ_DATA_FORMAT_INT16;
        header->record_length = static_cast<uint32_t>(m_top_parameters.record_length);
        header->record_number = m_record_number;
        header->record_start = static_cast<int64_t>(m_distribution(m_random_generator) * 1000);
        header->time_unit = TIME_UNIT;
        header->timestamp = m_timestamp;
        header->sampling_period = static_cast<uint64_t>(
            std::round(1.0 / (TIME_UNIT * m_clock_system_parameters.sampling_frequency)));

        if (!(m_record_number % 50))
            header->misc |= 0x1u;
        if (!(m_record_number % 30))
            header->misc |= 0x2u;

        header->timestamp_synchronization_counter = static_cast<uint16_t>(m_record_number / 100);
    }

private:
    /* A generator must override the `Generate()` method. */
    virtual void Generate() = 0;

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
                    std::round(1.0 / (TIME_UNIT * m_top_parameters.trigger_frequency)));
            }

            /* If the generator is not enabled, we sleep for 100 ms to still be
               able to process messages. Otherwise, we sleep for a the rest of
               the trigger period. */
            const auto delta = (std::chrono::high_resolution_clock::now() - start).count();
            const int wait_us = static_cast<int>(1000000.0 / std::max(0.5, m_top_parameters.trigger_frequency));
            const int remainder_us = m_enabled
                                         ? std::max(wait_us - static_cast<int>(delta / 1e3), 0)
                                         : 100000;

            /* We implement the sleep using the stop event to be able to
               immediately react to the event being set. */
            if (m_should_stop.wait_for(std::chrono::microseconds(remainder_us)) == std::future_status::ready)
                break;
        }
    }

    int SetParameters(GeneratorMessageId id, const nlohmann::json &json)
    {
        try
        {
            switch (id)
            {
            case GeneratorMessageId::SET_TOP_PARAMETERS:
                m_top_parameters = json.get<T>();
                return SCAPE_EOK;

            case GeneratorMessageId::SET_CLOCK_SYSTEM_PARAMETERS:
                m_clock_system_parameters = json.get<C>();
                return SCAPE_EOK;

            default:
                fprintf(stderr, "Unexpected message id %d.\n", static_cast<int>(id));
                return SCAPE_EINVAL;
            }
        }
        catch (const nlohmann::json::exception &e)
        {
            fprintf(stderr, "Failed to parse the parameter set %s.\n", e.what());
            return SCAPE_EINVAL;
        }
    }

    int GetParameters(GeneratorMessageId id, nlohmann::json &json)
    {
        switch (id)
        {
        case GeneratorMessageId::GET_TOP_PARAMETERS:
            json = T{};
            return SCAPE_EOK;

        case GeneratorMessageId::GET_CLOCK_SYSTEM_PARAMETERS:
            json = C{};
            return SCAPE_EOK;

        default:
            fprintf(stderr, "Unexpected message id %d.\n", static_cast<int>(id));
            return SCAPE_EINVAL;
        }
    }

    void ProcessMessages()
    {
        GeneratorMessage message;
        while (SCAPE_EOK == _WaitForMessage(message, 0))
        {
            switch (message.id)
            {
            case GeneratorMessageId::SET_TOP_PARAMETERS:
            case GeneratorMessageId::SET_CLOCK_SYSTEM_PARAMETERS:
            {
                /* Reset the random distribution when parameters are applied successfully. */
                message.result = SetParameters(message.id, message.json);
                if (message.result == SCAPE_EOK)
                    m_distribution = std::normal_distribution<double>(0, m_top_parameters.noise);
                break;
            }

            case GeneratorMessageId::GET_TOP_PARAMETERS:
            case GeneratorMessageId::GET_CLOCK_SYSTEM_PARAMETERS:
            {
                message.result = GetParameters(message.id, message.json);
                break;
            }

            case GeneratorMessageId::ENABLE:
            {
                m_enabled = true;
                m_timestamp = 0;
                m_record_number = 0;
                message.result = SCAPE_EOK;
                break;
            }

            case GeneratorMessageId::DISABLE:
            {
                m_enabled = false;
                message.result = SCAPE_EOK;
                break;
            }

            default:
            {
                fprintf(
                    stderr, "Generator: Unknown message id '%d'.\n", static_cast<int>(message.id));
                message.result = SCAPE_EINVAL;
                break;
            }
            }

            /* Always send a response. */
            _EmplaceMessage(std::move(message));
        }
    }
};
