#include "api_client.h"

#include "app_config.h"
#include "log.h"
#include "wifi_service.h"

#include "stm32l4xx_hal.h"
#include <stdio.h>
#include <string.h>

typedef struct
{
  int32_t whole;
  int32_t fraction;
} Decimal2;

static Decimal2 Decimal2_FromFloat(float value)
{
  Decimal2 decimal;
  int32_t scaled;

  scaled = (int32_t)((value * 100.0f) + ((value >= 0.0f) ? 0.5f : -0.5f));
  decimal.whole = scaled / 100;
  decimal.fraction = scaled % 100;
  if (decimal.fraction < 0)
  {
    decimal.fraction = -decimal.fraction;
  }

  return decimal;
}

static int WriteTelemetryJson(char *buffer, size_t buffer_size, const TelemetryData *telemetry, const uint8_t local_ip[4], const ApiClientContext *context)
{
  Decimal2 temperature = Decimal2_FromFloat(telemetry->temperature_c);
  Decimal2 raw_temperature = Decimal2_FromFloat(telemetry->temperature_raw_c);
  Decimal2 humidity = Decimal2_FromFloat(telemetry->humidity_percent);
  Decimal2 pressure = Decimal2_FromFloat(telemetry->pressure_hpa);
  Decimal2 temperature_offset = Decimal2_FromFloat(APP_TEMPERATURE_OFFSET_C);

  return snprintf(buffer,
                  buffer_size,
                  "{"
                  "\"device\":{"
                  "\"id\":\"%s\","
                  "\"firmware\":\"%s\","
                  "\"location\":\"%s\""
                  "},"
                  "\"runtime\":{"
                  "\"uptime_ms\":%lu,"
                  "\"sequence\":%lu,"
                  "\"success_count\":%lu,"
                  "\"failure_count\":%lu,"
                  "\"wifi_reconnect_count\":%lu,"
                  "\"consecutive_failure_count\":%lu,"
                  "\"last_success_uptime_ms\":%lu,"
                  "\"watchdog_enabled\":%u,"
                  "\"reset_reason\":\"%s\","
                  "\"last_error\":\"%s\","
                  "\"wifi_recovery_count\":%lu,"
                  "\"system_reset_request_count\":%lu"
                  "},"
                  "\"network\":{"
                  "\"ssid\":\"%s\","
                  "\"local_ip\":\"%u.%u.%u.%u\","
                  "\"api_host\":\"%s\","
                  "\"api_ip\":\"%u.%u.%u.%u\","
                  "\"api_port\":%u"
                  "},"
                  "\"config\":{"
                  "\"send_interval_ms\":%lu,"
                  "\"retry_interval_ms\":%lu,"
                  "\"temperature_offset_c\":%ld.%02ld,"
                  "\"sample_count\":%lu"
                  "},"
                  "\"sensors\":{"
                  "\"humidity_sensor_id\":%u,"
                  "\"pressure_sensor_id\":%u"
                  "},"
                  "\"telemetry\":{"
                  "\"temperature_c\":%ld.%02ld,"
                  "\"raw_temperature_c\":%ld.%02ld,"
                  "\"humidity_percent\":%ld.%02ld,"
                  "\"pressure_hpa\":%ld.%02ld"
                  "}"
                  "}",
                  APP_DEVICE_ID,
                  APP_FIRMWARE_VERSION,
                  APP_LOCATION,
                  HAL_GetTick(),
                  context->sequence,
                  context->success_count,
                  context->failure_count,
                  context->wifi_reconnect_count,
                  context->consecutive_failure_count,
                  context->last_success_uptime_ms,
                  context->watchdog_enabled,
                  (context->reset_reason != 0) ? context->reset_reason : "unknown",
                  (context->last_error != 0) ? context->last_error : "none",
                  context->wifi_recovery_count,
                  context->system_reset_request_count,
                  APP_WIFI_SSID,
                  local_ip[0],
                  local_ip[1],
                  local_ip[2],
                  local_ip[3],
                  APP_API_HOST,
                  APP_API_IP_ADDR_VALUE_0,
                  APP_API_IP_ADDR_VALUE_1,
                  APP_API_IP_ADDR_VALUE_2,
                  APP_API_IP_ADDR_VALUE_3,
                  APP_API_PORT,
                  APP_SEND_INTERVAL_MS,
                  APP_SEND_RETRY_INTERVAL_MS,
                  temperature_offset.whole,
                  temperature_offset.fraction,
                  telemetry->sample_count,
                  telemetry->humidity_sensor_id,
                  telemetry->pressure_sensor_id,
                  temperature.whole,
                  temperature.fraction,
                  raw_temperature.whole,
                  raw_temperature.fraction,
                  humidity.whole,
                  humidity.fraction,
                  pressure.whole,
                  pressure.fraction);
}

static int WriteHttpRequest(char *buffer, size_t buffer_size, const char *body)
{
  return snprintf(buffer,
                  buffer_size,
                  "POST %s HTTP/1.1\r\n"
                  "Host: %s\r\n"
                  "User-Agent: %s\r\n"
                  "Content-Type: application/json\r\n"
                  "Content-Length: %u\r\n"
                  "Connection: close\r\n"
                  "\r\n"
                  "%s",
                  APP_API_PATH,
                  APP_API_HOST,
                  APP_DEVICE_ID,
                  (unsigned int)strlen(body),
                  body);
}

static uint8_t IsHttpSuccessResponse(const char *response)
{
  const char *status_code;

  if (response == 0)
  {
    return 0U;
  }

  if (strncmp(response, "HTTP/", 5U) != 0)
  {
    return 0U;
  }

  status_code = strchr(response, ' ');
  if (status_code == 0)
  {
    return 0U;
  }

  status_code++;
  return (strncmp(status_code, "200", 3U) == 0) ? 1U : 0U;
}

ApiClientStatus ApiClient_SendTelemetry(const TelemetryData *telemetry, const uint8_t local_ip[4], const ApiClientContext *context)
{
  uint8_t api_ip[4] = APP_API_IP_ADDR;
  char body[1200];
  char request[1600];
  uint8_t response[256];
  uint16_t sent_length = 0;
  uint16_t received_length = 0;
  int body_length;
  int request_length;
  ApiClientStatus status = API_CLIENT_STATUS_ERROR;

  if ((telemetry == 0) || (local_ip == 0) || (context == 0))
  {
    return API_CLIENT_STATUS_ERROR;
  }

  body_length = WriteTelemetryJson(body, sizeof(body), telemetry, local_ip, context);
  if ((body_length < 0) || ((size_t)body_length >= sizeof(body)))
  {
    Log_Error("Telemetry JSON is too large");
    return API_CLIENT_STATUS_ERROR;
  }

  request_length = WriteHttpRequest(request, sizeof(request), body);
  if ((request_length < 0) || ((size_t)request_length >= sizeof(request)))
  {
    Log_Error("HTTP request is too large");
    return API_CLIENT_STATUS_ERROR;
  }

  Log_Info("Opening TCP connection to %s:%u (%u.%u.%u.%u)",
           APP_API_HOST,
           APP_API_PORT,
           api_ip[0],
           api_ip[1],
           api_ip[2],
           api_ip[3]);

  if (WifiService_OpenTcpClient(APP_SOCKET_ID, APP_API_HOST, api_ip, APP_API_PORT, APP_SOCKET_LOCAL_PORT) != WIFI_SERVICE_STATUS_OK)
  {
    Log_Error("Unable to open API connection");
    return API_CLIENT_STATUS_ERROR;
  }

  if (WifiService_Send(APP_SOCKET_ID, (const uint8_t *)request, (uint16_t)request_length, &sent_length, APP_NETWORK_TIMEOUT_MS) == WIFI_SERVICE_STATUS_OK)
  {
    Log_Info("Sent %u/%u bytes to API", sent_length, (unsigned int)request_length);

    if (WifiService_Receive(APP_SOCKET_ID, response, sizeof(response) - 1U, &received_length, APP_NETWORK_TIMEOUT_MS) == WIFI_SERVICE_STATUS_OK)
    {
      response[received_length] = '\0';
      Log_Info("API response received (%u bytes)", received_length);
      if (IsHttpSuccessResponse((char *)response) != 0U)
      {
        status = API_CLIENT_STATUS_OK;
      }
      else
      {
        Log_Error("API returned an unexpected HTTP response: %.48s", response);
      }
    }
    else
    {
      Log_Warn("No API response received before timeout");
    }
  }
  else
  {
    Log_Error("Unable to send request to API");
  }

  if (WifiService_CloseClient(APP_SOCKET_ID) != WIFI_SERVICE_STATUS_OK)
  {
    Log_Warn("Unable to close API connection cleanly");
  }

  return status;
}
