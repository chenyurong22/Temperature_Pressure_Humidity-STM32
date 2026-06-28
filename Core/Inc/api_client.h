#ifndef API_CLIENT_H
#define API_CLIENT_H

#include "telemetry.h"
#include <stdint.h>

typedef enum
{
  API_CLIENT_STATUS_OK = 0,
  API_CLIENT_STATUS_ERROR
} ApiClientStatus;

typedef struct
{
  uint32_t sequence;
  uint32_t success_count;
  uint32_t failure_count;
  uint32_t consecutive_failure_count;
  uint32_t wifi_reconnect_count;
  uint32_t wifi_recovery_count;
  uint32_t system_reset_request_count;
  uint32_t last_success_uptime_ms;
  uint8_t watchdog_enabled;
  const char *last_error;
  const char *reset_reason;
} ApiClientContext;

ApiClientStatus ApiClient_SendTelemetry(const TelemetryData *telemetry, const uint8_t local_ip[4], const ApiClientContext *context);

#endif
