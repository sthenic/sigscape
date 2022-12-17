#include "mock_system_manager.h"
#include <cinttypes>

#define SENSOR_ID_EOM (0u)

#define SENSOR_ID_0V95 (1u)
#define SENSOR_ID_3V3 (2u)
#define SENSOR_ID_5V0 (3u)
#define SENSOR_ID_2V6A_NEG (4u)

#define SENSOR_ID_CURRENT_0V95 (10u)
#define SENSOR_ID_CURRENT_3V3 (11u)
#define SENSOR_ID_CURRENT_5V0 (12u)
#define SENSOR_ID_CURRENT_2V6A_NEG (13u)

#define SENSOR_ID_TEMPERATURE_ADC1 (20u)
#define SENSOR_ID_TEMPERATURE_ADC2 (21u)
#define SENSOR_ID_TEMPERATURE_FPGA (22u)
#define SENSOR_ID_TEMPERATURE_DCDC (23u)

#define SENSOR_ID_POWER_0V95 (30u)
#define SENSOR_ID_POWER_3V3 (31u)
#define SENSOR_ID_POWER_5V0 (32u)
#define SENSOR_ID_POWER_2V6A_NEG (33u)

#define SENSOR_GROUP_ID_EOM (0u)
#define SENSOR_GROUP_ID_VOLTAGE (1u)
#define SENSOR_GROUP_ID_CURRENT (2u)
#define SENSOR_GROUP_ID_TEMPERATURE (3u)
#define SENSOR_GROUP_ID_POWER (4u)

MockSystemManager::MockSystemManager() :
    m_random_generator(std::chrono::steady_clock::now().time_since_epoch().count()),
    m_sensor_map{SENSOR_ID_0V95,
                 SENSOR_ID_3V3,
                 SENSOR_ID_5V0,
                 SENSOR_ID_2V6A_NEG,
                 SENSOR_ID_CURRENT_0V95,
                 SENSOR_ID_CURRENT_3V3,
                 SENSOR_ID_CURRENT_5V0,
                 SENSOR_ID_CURRENT_2V6A_NEG,
                 SENSOR_ID_TEMPERATURE_ADC1,
                 SENSOR_ID_TEMPERATURE_ADC2,
                 SENSOR_ID_TEMPERATURE_FPGA,
                 SENSOR_ID_TEMPERATURE_DCDC,
                 SENSOR_ID_POWER_0V95,
                 SENSOR_ID_POWER_3V3,
                 SENSOR_ID_POWER_5V0,
                 SENSOR_ID_POWER_2V6A_NEG,
                 SENSOR_ID_EOM},

    m_sensor_group_information{
        {SENSOR_GROUP_ID_VOLTAGE, {SENSOR_GROUP_ID_VOLTAGE, "Voltage"}},
        {SENSOR_GROUP_ID_CURRENT, {SENSOR_GROUP_ID_CURRENT, "Current"}},
        {SENSOR_GROUP_ID_TEMPERATURE, {SENSOR_GROUP_ID_TEMPERATURE, "Temperature"}},
        {SENSOR_GROUP_ID_POWER, {SENSOR_GROUP_ID_POWER, "Power"}}
    },

    m_sensor_information{
        {SENSOR_ID_0V95, {SENSOR_ID_0V95, SENSOR_GROUP_ID_VOLTAGE, "+0V95", "V"}},
        {SENSOR_ID_3V3, {SENSOR_ID_3V3, SENSOR_GROUP_ID_VOLTAGE, "+3V3", "V"}},
        {SENSOR_ID_5V0, {SENSOR_ID_5V0, SENSOR_GROUP_ID_VOLTAGE, "+5V0", "V"}},
        {SENSOR_ID_2V6A_NEG, {SENSOR_ID_2V6A_NEG, SENSOR_GROUP_ID_VOLTAGE, "-2V6", "V"}},
        {SENSOR_ID_CURRENT_0V95, {SENSOR_ID_CURRENT_0V95, SENSOR_GROUP_ID_CURRENT, "+0V95", "A"}},
        {SENSOR_ID_CURRENT_3V3, {SENSOR_ID_CURRENT_3V3, SENSOR_GROUP_ID_CURRENT, "+3V3", "A"}},
        {SENSOR_ID_CURRENT_5V0, {SENSOR_ID_CURRENT_5V0, SENSOR_GROUP_ID_CURRENT, "+5V0", "A"}},
        {SENSOR_ID_CURRENT_2V6A_NEG, {SENSOR_ID_CURRENT_2V6A_NEG, SENSOR_GROUP_ID_CURRENT, "-2V6", "A"}},
        {SENSOR_ID_TEMPERATURE_ADC1, {SENSOR_ID_TEMPERATURE_ADC1, SENSOR_GROUP_ID_TEMPERATURE, "ADC1", "degC"}},
        {SENSOR_ID_TEMPERATURE_ADC2, {SENSOR_ID_TEMPERATURE_ADC2, SENSOR_GROUP_ID_TEMPERATURE, "ADC2", "degC"}},
        {SENSOR_ID_TEMPERATURE_FPGA, {SENSOR_ID_TEMPERATURE_FPGA, SENSOR_GROUP_ID_TEMPERATURE, "FPGA", "degC"}},
        {SENSOR_ID_TEMPERATURE_DCDC, {SENSOR_ID_TEMPERATURE_DCDC, SENSOR_GROUP_ID_TEMPERATURE, "DCDC", "degC"}},
        {SENSOR_ID_POWER_0V95, {SENSOR_ID_POWER_0V95, SENSOR_GROUP_ID_POWER, "+0V95", "W"}},
        {SENSOR_ID_POWER_3V3, {SENSOR_ID_POWER_3V3, SENSOR_GROUP_ID_POWER, "+3V3", "W"}},
        {SENSOR_ID_POWER_5V0, {SENSOR_ID_POWER_5V0, SENSOR_GROUP_ID_POWER, "+5V0", "W"}},
        {SENSOR_ID_POWER_2V6A_NEG, {SENSOR_ID_POWER_2V6A_NEG, SENSOR_GROUP_ID_POWER, "-2V6", "W"}}
    },

    m_sensors{{SENSOR_ID_0V95, std::normal_distribution(0.95f, 0.1f)},
              {SENSOR_ID_3V3, std::normal_distribution(3.3f, 0.3f)},
              {SENSOR_ID_5V0, std::normal_distribution(5.0f, 0.5f)},
              {SENSOR_ID_2V6A_NEG, std::normal_distribution(-2.6f, 0.2f)},
              {SENSOR_ID_CURRENT_0V95, std::normal_distribution(10.0f, 0.7f)},
              {SENSOR_ID_CURRENT_3V3, std::normal_distribution(1.0f, 0.2f)},
              {SENSOR_ID_CURRENT_5V0, std::normal_distribution(0.68f, 0.1f)},
              {SENSOR_ID_CURRENT_2V6A_NEG, std::normal_distribution(0.32f, 0.1f)},
              {SENSOR_ID_TEMPERATURE_ADC1, std::normal_distribution(60.0f, 1.1f)},
              {SENSOR_ID_TEMPERATURE_ADC2, std::normal_distribution(62.4f, 1.0f)},
              {SENSOR_ID_TEMPERATURE_FPGA, std::normal_distribution(67.3f, 2.5f)},
              {SENSOR_ID_TEMPERATURE_DCDC, std::normal_distribution(55.0f, 1.1f)},
              {SENSOR_ID_POWER_0V95, std::normal_distribution(0.95f * 10.0f, 0.1f)},
              {SENSOR_ID_POWER_3V3, std::normal_distribution(3.3f * 1.0f, 0.1f)},
              {SENSOR_ID_POWER_5V0, std::normal_distribution(5.0f * 0.68f, 0.1f)},
              {SENSOR_ID_POWER_2V6A_NEG, std::normal_distribution(2.6f * 0.32f, 0.1f)}}
{
}

MockSystemManager::~MockSystemManager()
{}

void MockSystemManager::MainLoop()
{
    m_thread_exit_code = ADQR_EOK;
    for (;;)
    {
        /* Handle any incoming messages. This function blocks until there's a
           message, or the process is stopped.*/
        int result = HandleMessage();
        if (result != ADQR_EOK)
            return;
    }
}

int MockSystemManager::HandleMessage()
{
    /* Wait (indefinitely) for a new message. */
    SystemManagerMessage message;
    int result = m_write_queue.Read(message, -1);
    if (result != ADQR_EOK)
        return result;

    switch (message.cmd)
    {
    case SystemManagerCommand::SENSOR_GET_NOF_SENSORS:
    {
        /* -1 for the EOM */
        auto nof_sensors = static_cast<uint32_t>(m_sensor_map.size() - 1);
        m_read_queue.EmplaceWrite(&nof_sensors, sizeof(nof_sensors));
        break;
    }

    case SystemManagerCommand::SENSOR_GET_MAP:
    {
        m_read_queue.EmplaceWrite(m_sensor_map.data(),
                                  sizeof(m_sensor_map[0]) * m_sensor_map.size());
        break;
    }

    case SystemManagerCommand::SENSOR_GET_VALUE:
    {
        if (message.data.size() != sizeof(ArgSensorGetValue))
        {
            printf("Invalid arugment length for SENSOR_GET_VALUE: %zu != %zu.\n",
                    message.data.size(), sizeof(ArgSensorGetValue));
            m_read_queue.EmplaceWrite(-1);
        }

        auto arg = reinterpret_cast<ArgSensorGetValue *>(message.data.data());
        if (m_sensors.count(arg->id) > 0)
        {
            if (arg->format == 1)
            {
                float value = m_sensors.at(arg->id)(m_random_generator);
                m_read_queue.EmplaceWrite(&value, sizeof(value));
            }
            else
            {
                printf("Unsupported sensor format %" PRIu32 ".\n", arg->format);
                m_read_queue.EmplaceWrite(-1);
            }
        }
        else
        {
            printf("Unknown sensor id %" PRIu32 ".\n", arg->id);
            m_read_queue.EmplaceWrite(-1);
        }
        break;
    }

    case SystemManagerCommand::SENSOR_GET_INFO:
    {
        if (message.data.size() != sizeof(uint32_t))
        {
            printf("Invalid arugment length for SENSOR_GET_INFO: %zu != %zu.\n",
                    message.data.size(), sizeof(uint32_t));
            m_read_queue.EmplaceWrite(-1);
        }

        auto id = reinterpret_cast<const uint32_t *>(message.data.data());
        if (m_sensor_information.count(*id) > 0)
        {
            const auto &information = m_sensor_information.at(*id);
            m_read_queue.EmplaceWrite(&information, sizeof(information));
        }
        else
        {
            printf("Unknown sensor id %" PRIu32 ".\n", *id);
            m_read_queue.EmplaceWrite(-1);
        }
        break;
    }

    case SystemManagerCommand::SENSOR_GET_GROUP_INFO:
    {
        if (message.data.size() != sizeof(uint32_t))
        {
            printf("Invalid arugment length for SENSOR_GET_GROUP_INFO: %zu != %zu.\n",
                    message.data.size(), sizeof(uint32_t));
            m_read_queue.EmplaceWrite(-1);
        }

        auto id = reinterpret_cast<const uint32_t *>(message.data.data());
        if (m_sensor_group_information.count(*id) > 0)
        {
            const auto &information = m_sensor_group_information.at(*id);
            m_read_queue.EmplaceWrite(&information, sizeof(information));
        }
        else
        {
            printf("Unknown sensor id %" PRIu32 ".\n", *id);
            m_read_queue.EmplaceWrite(-1);
        }
        break;
    }
    default:
    {
        printf("Unsupported system manager command 0x%04X.\n", static_cast<int>(message.cmd));
        m_read_queue.EmplaceWrite(-1);
        break;
    }
    }

    return ADQR_EOK;
}
