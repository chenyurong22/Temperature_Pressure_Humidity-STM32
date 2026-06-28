#include "app.h"

#include "api_client.h"
#include "app_config.h"
#include "log.h"
#include "system_health.h"
#include "telemetry.h"
#include "wifi_service.h"

#include "stm32l4xx_hal.h"
#include <stdint.h>

typedef struct
{
  int32_t whole;
  int32_t fraction;
} AppDecimal2;

static uint8_t app_wifi_connected;
static uint8_t app_wifi_module_ready;
static uint8_t app_sensors_ready;
static uint32_t app_next_send_ms;
static uint32_t app_next_retry_ms;
static uint8_t app_local_ip[4];
static uint32_t app_sequence;
static uint32_t app_send_success_count;
static uint32_t app_send_failure_count;
static uint32_t app_consecutive_failure_count;
static uint32_t app_wifi_reconnect_count;
static uint32_t app_wifi_recovery_count;
static uint32_t app_system_reset_request_count;
static uint32_t app_last_success_uptime_ms;
static const char *app_last_error;

static AppDecimal2 AppDecimal2_FromFloat(float value)
{
  AppDecimal2 decimal;
  int32_t scaled = (int32_t)((value * 100.0f) + ((value >= 0.0f) ? 0.5f : -0.5f));

  decimal.whole = scaled / 100;
  decimal.fraction = scaled % 100;
  if (decimal.fraction < 0)
  {
    decimal.fraction = -decimal.fraction;
  }

  return decimal;
}

static uint8_t TimeReached(uint32_t timestamp_ms)
{
  return ((int32_t)(HAL_GetTick() - timestamp_ms) >= 0) ? 1U : 0U;
}

static void ScheduleRetry(void)
{
  app_next_retry_ms = HAL_GetTick() + APP_RETRY_INTERVAL_MS;
}

static void ScheduleNextSend(void)
{
  app_next_send_ms = HAL_GetTick() + APP_SEND_INTERVAL_MS;
}

static void ScheduleSendRetry(void)
{
  app_next_send_ms = HAL_GetTick() + APP_SEND_RETRY_INTERVAL_MS;
}

static void RequestSystemReset(const char *reason)
{
  app_system_reset_request_count++;
  app_last_error = reason;
  Log_Error("System reset requested after %lu consecutive failure(s): %s",
            app_consecutive_failure_count,
            reason);
  HAL_Delay(100U);
  SystemHealth_RequestReset();
}

static void RecoverWifi(const char *reason)
{
  app_wifi_recovery_count++;
  Log_Warn("WiFi recovery #%lu after %lu consecutive failure(s): %s",
           app_wifi_recovery_count,
           app_consecutive_failure_count,
           reason);

  (void)WifiService_CloseClient(APP_SOCKET_ID);
  WifiService_Disconnect();
  (void)WifiService_ResetModule();

  app_wifi_connected = 0U;
  app_wifi_module_ready = 0U;
  ScheduleRetry();
}

static void RegisterFailure(const char *reason, uint8_t count_as_send_failure)
{
  app_last_error = reason;
  app_consecutive_failure_count++;

  if (count_as_send_failure != 0U)
  {
    app_send_failure_count++;
  }

  Log_Warn("Failure #%lu: %s", app_consecutive_failure_count, reason);

  if (app_consecutive_failure_count >= APP_SYSTEM_RESET_FAILURE_THRESHOLD)
  {
    RequestSystemReset(reason);
  }

  if ((app_consecutive_failure_count % APP_WIFI_RECOVERY_FAILURE_THRESHOLD) == 0U)
  {
    RecoverWifi(reason);
  }
}

static void RegisterSuccess(void)
{
  app_consecutive_failure_count = 0U;
  app_last_error = "none";
  app_last_success_uptime_ms = HAL_GetTick();
}

static void BuildApiContext(ApiClientContext *context)
{
  context->sequence = app_sequence;
  context->success_count = app_send_success_count;
  context->failure_count = app_send_failure_count;
  context->consecutive_failure_count = app_consecutive_failure_count;
  context->wifi_reconnect_count = app_wifi_reconnect_count;
  context->wifi_recovery_count = app_wifi_recovery_count;
  context->system_reset_request_count = app_system_reset_request_count;
  context->last_success_uptime_ms = app_last_success_uptime_ms;
  context->watchdog_enabled = SystemHealth_IsWatchdogEnabled();
  context->last_error = app_last_error;
  context->reset_reason = SystemHealth_GetResetReasonText();
}

static uint8_t EnsureWifiConnected(void)
{
  if (app_wifi_module_ready == 0U)
  {
    if (!TimeReached(app_next_retry_ms))
    {
      return 0U;
    }

    Log_Info("Initializing WiFi module");
    if (WifiService_Init() != WIFI_SERVICE_STATUS_OK)
    {
      Log_Error("WiFi module initialization failed");
      RegisterFailure("wifi_init_failed", 0U);
      ScheduleRetry();
      return 0U;
    }

    app_wifi_module_ready = 1U;
    Log_Info("WiFi module initialized");
  }

  if (app_wifi_connected != 0U)
  {
    return 1U;
  }

  if (!TimeReached(app_next_retry_ms))
  {
    return 0U;
  }

  Log_Info("Connecting to WiFi SSID \"%s\"", APP_WIFI_SSID);
  if (WifiService_Connect() != WIFI_SERVICE_STATUS_OK)
  {
    Log_Error("WiFi connection failed");
    app_wifi_connected = 0U;
    RegisterFailure("wifi_connect_failed", 0U);
    ScheduleRetry();
    return 0U;
  }

  if (WifiService_GetLocalIp(app_local_ip) != WIFI_SERVICE_STATUS_OK)
  {
    Log_Error("Unable to read local IP address");
    WifiService_Disconnect();
    app_wifi_connected = 0U;
    RegisterFailure("wifi_ip_read_failed", 0U);
    ScheduleRetry();
    return 0U;
  }

  app_wifi_connected = 1U;
  app_wifi_reconnect_count++;
  Log_Info("WiFi connected with local IP %u.%u.%u.%u",
           app_local_ip[0],
           app_local_ip[1],
           app_local_ip[2],
           app_local_ip[3]);

  return 1U;
}

static void SendTelemetryCycle(void)
{
  TelemetryData telemetry;
  AppDecimal2 temperature;
  AppDecimal2 raw_temperature;
  AppDecimal2 humidity;
  AppDecimal2 pressure;
  ApiClientContext api_context;

  if (app_sensors_ready == 0U)
  {
    Log_Info("Initializing sensors");
    if (Telemetry_Init() != TELEMETRY_STATUS_OK)
    {
      Log_Error("Sensor initialization failed");
      RegisterFailure("sensor_init_failed", 0U);
      ScheduleNextSend();
      return;
    }

    app_sensors_ready = 1U;
    Log_Info("Sensors initialized");
  }

  if (Telemetry_Read(&telemetry) != TELEMETRY_STATUS_OK)
  {
    Log_Error("Unable to read telemetry");
    RegisterFailure("telemetry_read_failed", 0U);
    ScheduleNextSend();
    return;
  }

  temperature = AppDecimal2_FromFloat(telemetry.temperature_c);
  raw_temperature = AppDecimal2_FromFloat(telemetry.temperature_raw_c);
  humidity = AppDecimal2_FromFloat(telemetry.humidity_percent);
  pressure = AppDecimal2_FromFloat(telemetry.pressure_hpa);

  Log_Info("Telemetry temp=%ld.%02ldC raw=%ld.%02ldC hum=%ld.%02ld%% pressure=%ld.%02ldhPa",
           temperature.whole,
           temperature.fraction,
           raw_temperature.whole,
           raw_temperature.fraction,
           humidity.whole,
           humidity.fraction,
           pressure.whole,
           pressure.fraction);

  app_sequence++;
  BuildApiContext(&api_context);

  if (ApiClient_SendTelemetry(&telemetry, app_local_ip, &api_context) != API_CLIENT_STATUS_OK)
  {
    Log_Error("Telemetry send failed, API retry scheduled");
    RegisterFailure("api_send_failed", 1U);
    ScheduleSendRetry();
    return;
  }

  app_send_success_count++;
  RegisterSuccess();
  Log_Info("Telemetry sent successfully");
  ScheduleNextSend();
}

void App_Init(void)
{
  app_wifi_connected = 0U;
  app_wifi_module_ready = 0U;
  app_sensors_ready = 0U;
  app_next_send_ms = 0U;
  app_next_retry_ms = 0U;
  app_sequence = 0U;
  app_send_success_count = 0U;
  app_send_failure_count = 0U;
  app_consecutive_failure_count = 0U;
  app_wifi_reconnect_count = 0U;
  app_wifi_recovery_count = 0U;
  app_system_reset_request_count = 0U;
  app_last_success_uptime_ms = 0U;
  app_last_error = "none";

  Log_Info("Starting %s", APP_DEVICE_ID);
  Log_Info("Reset reason=%s watchdog=%s",
           SystemHealth_GetResetReasonText(),
           (SystemHealth_IsWatchdogEnabled() != 0U) ? "enabled" : "disabled");

  if (Telemetry_Init() != TELEMETRY_STATUS_OK)
  {
    Log_Error("Sensor initialization failed");
    RegisterFailure("sensor_init_failed", 0U);
  }
  else
  {
    app_sensors_ready = 1U;
    Log_Info("Sensors initialized");
  }

  if (WifiService_Init() != WIFI_SERVICE_STATUS_OK)
  {
    Log_Error("WiFi module initialization failed");
    RegisterFailure("wifi_init_failed", 0U);
    ScheduleRetry();
  }
  else
  {
    app_wifi_module_ready = 1U;
    Log_Info("WiFi module initialized");
  }
}

void App_Loop(void)
{
  if (EnsureWifiConnected() == 0U)
  {
    SystemHealth_WatchdogRefresh();
    HAL_Delay(100U);
    return;
  }

  if (TimeReached(app_next_send_ms))
  {
    SendTelemetryCycle();
  }

  SystemHealth_WatchdogRefresh();
  HAL_Delay(100U);
}
