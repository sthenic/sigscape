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
#define SENSOR_ID_TEMPERATURE_ERROR (24u) /* Sensor always returning an error (for testing). */

#define SENSOR_ID_POWER_0V95 (30u)
#define SENSOR_ID_POWER_3V3 (31u)
#define SENSOR_ID_POWER_5V0 (32u)
#define SENSOR_ID_POWER_2V6A_NEG (33u)

#define SENSOR_GROUP_ID_EOM (0u)
#define SENSOR_GROUP_ID_VOLTAGE (1u)
#define SENSOR_GROUP_ID_CURRENT (2u)
#define SENSOR_GROUP_ID_TEMPERATURE (3u)
#define SENSOR_GROUP_ID_POWER (4u)

#define BOOT_ID_EOM (0u)

#define BOOT_ID_CLOCK (1u)
#define BOOT_ID_SPI (2u)
#define BOOT_ID_I2C (3u)
#define BOOT_ID_REGULATORS (4u)
#define BOOT_ID_ERROR (5u)

MockSystemManager::MockSystemManager() :
    m_random_generator(
        static_cast<uint32_t>(std::chrono::steady_clock::now().time_since_epoch().count())),
    m_sensor_map{
        SENSOR_ID_0V95,
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
        SENSOR_ID_TEMPERATURE_ERROR,
        SENSOR_ID_POWER_0V95,
        SENSOR_ID_POWER_3V3,
        SENSOR_ID_POWER_5V0,
        SENSOR_ID_POWER_2V6A_NEG,
        SENSOR_ID_EOM
    },

    m_boot_map{
        BOOT_ID_CLOCK,
        BOOT_ID_SPI,
        BOOT_ID_I2C,
        BOOT_ID_REGULATORS,
        BOOT_ID_ERROR,
        BOOT_ID_EOM
    },

    m_boot_information{
        {BOOT_ID_CLOCK, {BOOT_ID_CLOCK, 0, "Clock system"}},
        {BOOT_ID_SPI, {BOOT_ID_SPI, 0, "SPI bus"}},
        {BOOT_ID_I2C, {BOOT_ID_I2C, 0, "I2C bus"}},
        {BOOT_ID_REGULATORS, {BOOT_ID_REGULATORS, 0, "Voltage regulators"}},
        {BOOT_ID_ERROR, {BOOT_ID_ERROR, -344, "Deliberate error"}}
    },

    m_sensor_group_information{
        {SENSOR_GROUP_ID_VOLTAGE, {SENSOR_GROUP_ID_VOLTAGE, "Voltage"}},
        {SENSOR_GROUP_ID_CURRENT, {SENSOR_GROUP_ID_CURRENT, "Current"}},
        {SENSOR_GROUP_ID_TEMPERATURE, {SENSOR_GROUP_ID_TEMPERATURE, "Temperature"}},
        {SENSOR_GROUP_ID_POWER, {SENSOR_GROUP_ID_POWER, "Power"}}
    },

    m_sensor_information{
        {SENSOR_ID_0V95, {SENSOR_ID_0V95, "+0V95", "V", SENSOR_GROUP_ID_VOLTAGE, {}}},
        {SENSOR_ID_3V3, {SENSOR_ID_3V3, "+3V3", "V", SENSOR_GROUP_ID_VOLTAGE, {}}},
        {SENSOR_ID_5V0, {SENSOR_ID_5V0, "+5V0", "V", SENSOR_GROUP_ID_VOLTAGE, {}}},
        {SENSOR_ID_2V6A_NEG, {SENSOR_ID_2V6A_NEG, "-2V6", "V", SENSOR_GROUP_ID_VOLTAGE, {}}},
        {SENSOR_ID_CURRENT_0V95, {SENSOR_ID_CURRENT_0V95, "+0V95 output current", "A", SENSOR_GROUP_ID_CURRENT, {}}},
        {SENSOR_ID_CURRENT_3V3, {SENSOR_ID_CURRENT_3V3, "+3V3 current", "A", SENSOR_GROUP_ID_CURRENT, {}}},
        {SENSOR_ID_CURRENT_5V0, {SENSOR_ID_CURRENT_5V0, "+5V0 current", "A", SENSOR_GROUP_ID_CURRENT, {}}},
        {SENSOR_ID_CURRENT_2V6A_NEG, {SENSOR_ID_CURRENT_2V6A_NEG, "-2V6 current", "A", SENSOR_GROUP_ID_CURRENT, {}}},
        {SENSOR_ID_TEMPERATURE_ADC1, {SENSOR_ID_TEMPERATURE_ADC1, "ADC1 temperature", "degC", SENSOR_GROUP_ID_TEMPERATURE, {}}},
        {SENSOR_ID_TEMPERATURE_ADC2, {SENSOR_ID_TEMPERATURE_ADC2, "ADC2 temperature", "degC", SENSOR_GROUP_ID_TEMPERATURE, {}}},
        {SENSOR_ID_TEMPERATURE_FPGA, {SENSOR_ID_TEMPERATURE_FPGA, "FPGA temperature", "degC", SENSOR_GROUP_ID_TEMPERATURE, {}}},
        {SENSOR_ID_TEMPERATURE_DCDC, {SENSOR_ID_TEMPERATURE_DCDC, "DCDC temperature", "degC", SENSOR_GROUP_ID_TEMPERATURE, {}}},
        {SENSOR_ID_TEMPERATURE_ERROR, {SENSOR_ID_TEMPERATURE_ERROR, "Error temperature", "degC", SENSOR_GROUP_ID_TEMPERATURE, {}}},
        {SENSOR_ID_POWER_0V95, {SENSOR_ID_POWER_0V95, "+0V95 power", "W", SENSOR_GROUP_ID_POWER, {}}},
        {SENSOR_ID_POWER_3V3, {SENSOR_ID_POWER_3V3, "+3V3 power", "W", SENSOR_GROUP_ID_POWER, {}}},
        {SENSOR_ID_POWER_5V0, {SENSOR_ID_POWER_5V0, "+5V0 power", "W", SENSOR_GROUP_ID_POWER, {}}},
        {SENSOR_ID_POWER_2V6A_NEG, {SENSOR_ID_POWER_2V6A_NEG, "-2V6 power", "W", SENSOR_GROUP_ID_POWER, {}}}
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
{
    Stop();
}

void MockSystemManager::MainLoop()
{
    m_thread_exit_code = SCAPE_EOK;
    for (;;)
    {
        /* Handle any incoming messages. This function blocks until there's a
           message, or the process is stopped. */
        int result = HandleMessage();
        if (result != SCAPE_EOK)
            return;
    }
}

int MockSystemManager::HandleMessage()
{
    /* Wait (indefinitely) for a new message. */
    StampedMessage message;
    int result = _WaitForMessage(message, -1);
    if (result != SCAPE_EOK)
        return result;

    switch (message.contents.cmd)
    {
    case SystemManagerCommand::SENSOR_GET_NOF_SENSORS:
    {
        /* -1 for the EOM */
        auto nof_sensors = static_cast<uint32_t>(m_sensor_map.size() - 1);
        _EmplaceMessage(message.id, &nof_sensors, sizeof(nof_sensors));
        break;
    }

    case SystemManagerCommand::SENSOR_GET_MAP:
    {
        _EmplaceMessage(message.id, m_sensor_map.data(),
                        sizeof(m_sensor_map[0]) * m_sensor_map.size());
        break;
    }

    case SystemManagerCommand::SENSOR_GET_VALUE:
    {
        if (message.contents.data.size() != sizeof(ArgSensorGetValue))
        {
            fprintf(stderr, "Invalid argument length for SENSOR_GET_VALUE: %zu != %zu.\n",
                    message.contents.data.size(), sizeof(ArgSensorGetValue));
            _EmplaceMessage(message.id, -1);
        }

        /* Intentionally return an error for one of the sensors. */
        auto arg = reinterpret_cast<ArgSensorGetValue *>(message.contents.data.data());
        if (arg->id == SENSOR_ID_TEMPERATURE_ERROR)
        {
            _EmplaceMessage(message.id, -271);
            break;
        }

        if (m_sensors.count(arg->id) > 0)
        {
            if (arg->format == SENSOR_FORMAT_FLOAT)
            {
                float value = m_sensors.at(arg->id)(m_random_generator);
                _EmplaceMessage(message.id, &value, sizeof(value));
            }
            else
            {
                fprintf(stderr, "Unsupported sensor format %" PRIu32 ".\n", arg->format);
                _EmplaceMessage(message.id, -1);
            }
        }
        else
        {
            fprintf(stderr, "Unknown sensor id %" PRIu32 ".\n", arg->id);
            _EmplaceMessage(message.id, -1);
        }
        break;
    }

    case SystemManagerCommand::SENSOR_GET_INFO:
    {
        if (message.contents.data.size() != sizeof(uint32_t))
        {
            fprintf(stderr, "Invalid argument length for SENSOR_GET_INFO: %zu != %zu.\n",
                    message.contents.data.size(), sizeof(uint32_t));
            _EmplaceMessage(message.id, -1);
        }

        auto id = reinterpret_cast<const uint32_t *>(message.contents.data.data());
        if (m_sensor_information.count(*id) > 0)
        {
            const auto &information = m_sensor_information.at(*id);
            _EmplaceMessage(message.id, &information, sizeof(information));
        }
        else
        {
            fprintf(stderr, "Unknown sensor id %" PRIu32 ".\n", *id);
            _EmplaceMessage(message.id, -1);
        }
        break;
    }

    case SystemManagerCommand::SENSOR_GET_GROUP_INFO:
    {
        if (message.contents.data.size() != sizeof(uint32_t))
        {
            fprintf(stderr, "Invalid argument length for SENSOR_GET_GROUP_INFO: %zu != %zu.\n",
                    message.contents.data.size(), sizeof(uint32_t));
            _EmplaceMessage(message.id, -1);
        }

        auto id = *reinterpret_cast<const uint32_t *>(message.contents.data.data());
        if (m_sensor_group_information.count(id) > 0)
        {
            const auto &information = m_sensor_group_information.at(id);
            _EmplaceMessage(message.id, &information, sizeof(information));
        }
        else
        {
            fprintf(stderr, "Unknown sensor id %" PRIu32 ".\n", id);
            _EmplaceMessage(message.id, -1);
        }
        break;
    }

    case SystemManagerCommand::BOOT_GET_NOF_ENTRIES:
    {
        /* -1 for the EOM */
        auto nof_entries = static_cast<uint32_t>(m_boot_map.size() - 1);
        _EmplaceMessage(message.id, &nof_entries, sizeof(nof_entries));
        break;
    }

    case SystemManagerCommand::BOOT_GET_MAP:
    {
        _EmplaceMessage(message.id, m_boot_map.data(), sizeof(m_boot_map[0]) * m_boot_map.size());
        break;
    }

    case SystemManagerCommand::BOOT_GET_INFO:
    {
        if (message.contents.data.size() != sizeof(uint32_t))
        {
            fprintf(stderr, "Invalid argument length for BOOT_GET_INFO: %zu != %zu.\n",
                    message.contents.data.size(), sizeof(uint32_t));
            _EmplaceMessage(message.id, -1);
        }

        auto id = *reinterpret_cast<const uint32_t *>(message.contents.data.data());
        if (m_boot_information.count(id) > 0)
        {
            const auto &information = m_boot_information.at(id);
            _EmplaceMessage(message.id, &information, sizeof(information));
        }
        else
        {
            fprintf(stderr, "Unknown boot id %" PRIu32 ".\n", id);
            _EmplaceMessage(message.id, -1);
        }
        break;
    }

    case SystemManagerCommand::GET_STATE:
    {
        int32_t state = 10;
        _EmplaceMessage(message.id, &state, sizeof(state));
        break;
    }

    case SystemManagerCommand::GET_STATE_INFO:
    {
        struct SystemManagerStateInformation information = {10, "Done"};
        _EmplaceMessage(message.id, &information, sizeof(information));
        break;
    }

    default:
    {
        fprintf(
            stderr, "Unsupported system manager command 0x%04X.\n",
            static_cast<int>(message.contents.cmd));
        _EmplaceMessage(message.id, -1);
        break;
    }
    }

    return SCAPE_EOK;
}
