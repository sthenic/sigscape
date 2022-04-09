/* This header file is used instead of ADQAPI.h. */

#ifndef ADQAPI_SIMULATION_H_GV91XI
#define ADQAPI_SIMULATION_H_GV91XI

#include <cstdint>

#define ADQ_MAX_NOF_CHANNELS 2

struct ADQGen4RecordHeader
{
  uint8_t version_major;
  uint8_t version_minor;
  uint16_t timestamp_synchronization_counter;
  uint16_t general_purpose_start;
  uint16_t general_purpose_stop;
  uint64_t timestamp;
  int64_t record_start;
  uint32_t record_length;
  uint8_t user_id;
  uint8_t reserved0;
  uint16_t record_status;
  uint32_t record_number;
  uint8_t channel;
  uint8_t data_format;
  char serial_number[10];
  uint64_t sampling_period;
  float time_unit;
  uint32_t reserved1;
};

#endif
