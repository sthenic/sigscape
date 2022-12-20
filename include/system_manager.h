#ifndef SYSTEM_MANAGER_H_DI4WW9
#define SYSTEM_MANAGER_H_DI4WW9

#include <cstdint>

/* Since the ADQAPI does not provide these, we have to define our own. This is
   only a small subset of the system manager's protocol and can change at any
   time. */

enum SystemManagerCommand
{
  BOOT_GET_NOF_ENTRIES = 0x0020,
  BOOT_GET_MAP = 0x0021,
  BOOT_GET_INFO = 0x0022,
  GET_STATE = 0x0030,
  GET_STATE_INFO = 0x0035,
  SENSOR_GET_NOF_SENSORS = 0x0300,
  SENSOR_GET_MAP = 0x0301,
  SENSOR_GET_VALUE = 0x0303,
  SENSOR_GET_INFO = 0x0307,
  SENSOR_GET_GROUP_INFO = 0x0308
};

struct SystemManagerSensorInformation
{
    uint32_t id;
    char label[32];
    char unit[8];
    uint32_t group_id;
    uint8_t reserved[16];
};

struct SystemManagerSensorGroupInformation
{
    uint32_t id;
    char label[32];
};

struct SystemManagerSensorCaptureSample
{
  uint32_t id;
  uint32_t time;
  float value;
};

struct SystemManagerBootInformation
{
    uint32_t id;
    char label[32];
    int32_t status;
};

struct SystemManagerStateInformation
{
    char label[32];
};

#define SENSOR_FORMAT_INT (0u)
#define SENSOR_FORMAT_FLOAT (1u)

struct ArgSensorGetValue
{
    uint32_t id;
    uint32_t format;
};

#endif
