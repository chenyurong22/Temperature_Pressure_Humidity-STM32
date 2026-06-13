#ifndef TELEMETRY_H
#define TELEMETRY_H

#include <stdint.h>

typedef struct
{
  float temperature_raw_c;
  float temperature_c;
  float humidity_percent;
  float pressure_hpa;
  uint8_t humidity_sensor_id;
  uint8_t pressure_sensor_id;
  uint32_t sample_count;
} TelemetryData;

typedef enum
{
  TELEMETRY_STATUS_OK = 0,
  TELEMETRY_STATUS_ERROR
} TelemetryStatus;

TelemetryStatus Telemetry_Init(void);
TelemetryStatus Telemetry_Read(TelemetryData *data);

#endif
