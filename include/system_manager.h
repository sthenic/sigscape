#ifndef SYSTEM_MANAGER_H_DI4WW9
#define SYSTEM_MANAGER_H_DI4WW9

#include <cstdint>

/* Since the ADQAPI does not provide these, we have to define our own. This is
   only a small subset of the system manager's protocol and can change at any
   time. */

enum SystemManagerCommand
{
  SENSOR_GET_NOF_SENSORS = 0x0300,
  SENSOR_GET_MAP = 0x0301,
  SENSOR_GET_VALUE = 0x0303,
  SENSOR_GET_INFO = 0x0307,
  SENSOR_GET_GROUP_INFO = 0x0308
};

struct SystemManagerSensorInformation
{
    uint32_t id;
    uint32_t group_id;
    char label[16];
    char unit[8];
};

struct SystemManagerSensorGroupInformation
{
    uint32_t id;
    char label[16];
};

#define SENSOR_FORMAT_INT (0u)
#define SENSOR_FORMAT_FLOAT (1u)

struct ArgSensorGetValue
{
    uint32_t id;
    uint32_t format;
};

struct ArgSensorGetInfo
{
    uint32_t id;
};

struct ArgSensorGetGroupInfo
{
    uint32_t id;
};

#endif
