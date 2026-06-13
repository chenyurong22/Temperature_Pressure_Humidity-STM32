#include "telemetry.h"

#include "app_config.h"
#include "stm32l475e_iot01_hsensor.h"
#include "stm32l475e_iot01_psensor.h"
#include "stm32l475e_iot01_tsensor.h"
#include "stm32l4xx_hal.h"

TelemetryStatus Telemetry_Init(void)
{
  if (BSP_TSENSOR_Init() != 0U)
  {
    return TELEMETRY_STATUS_ERROR;
  }

  if (BSP_HSENSOR_Init() != 0U)
  {
    return TELEMETRY_STATUS_ERROR;
  }

  if (BSP_PSENSOR_Init() != 0U)
  {
    return TELEMETRY_STATUS_ERROR;
  }

  return TELEMETRY_STATUS_OK;
}

TelemetryStatus Telemetry_Read(TelemetryData *data)
{
  float temperature_sum = 0.0f;
  float humidity_sum = 0.0f;
  float pressure_sum = 0.0f;
  uint32_t sample_count;

  if (data == 0)
  {
    return TELEMETRY_STATUS_ERROR;
  }

  sample_count = APP_TELEMETRY_SAMPLE_COUNT;
  if (sample_count == 0U)
  {
    sample_count = 1U;
  }

  for (uint32_t sample = 0; sample < sample_count; sample++)
  {
    temperature_sum += BSP_TSENSOR_ReadTemp();
    humidity_sum += BSP_HSENSOR_ReadHumidity();
    pressure_sum += BSP_PSENSOR_ReadPressure();

    if ((sample + 1U) < sample_count)
    {
      HAL_Delay(APP_TELEMETRY_SAMPLE_DELAY_MS);
    }
  }

  data->temperature_raw_c = temperature_sum / (float)sample_count;
  data->temperature_c = data->temperature_raw_c - APP_TEMPERATURE_OFFSET_C;
  data->humidity_percent = humidity_sum / (float)sample_count;
  data->pressure_hpa = pressure_sum / (float)sample_count;
  data->humidity_sensor_id = BSP_HSENSOR_ReadID();
  data->pressure_sensor_id = BSP_PSENSOR_ReadID();
  data->sample_count = sample_count;

  return TELEMETRY_STATUS_OK;
}
