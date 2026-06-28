#include "system_health.h"

#include "stm32l4xx_hal.h"

#define SYSTEM_HEALTH_IWDG_KEY_ENABLE 0xCCCCU
#define SYSTEM_HEALTH_IWDG_KEY_RELOAD 0xAAAAU
#define SYSTEM_HEALTH_IWDG_KEY_WRITE_ACCESS 0x5555U
#define SYSTEM_HEALTH_IWDG_PRESCALER_256 6U
#define SYSTEM_HEALTH_IWDG_RELOAD_MAX 0x0FFFU
#define SYSTEM_HEALTH_LSI_READY_TIMEOUT_MS 100U

static SystemHealthResetReason system_health_reset_reason = SYSTEM_HEALTH_RESET_UNKNOWN;
static uint8_t system_health_watchdog_enabled;

static SystemHealthResetReason ReadResetReason(void)
{
  if (__HAL_RCC_GET_FLAG(RCC_FLAG_IWDGRST) != RESET)
  {
    return SYSTEM_HEALTH_RESET_IWDG;
  }

  if (__HAL_RCC_GET_FLAG(RCC_FLAG_WWDGRST) != RESET)
  {
    return SYSTEM_HEALTH_RESET_WWDG;
  }

  if (__HAL_RCC_GET_FLAG(RCC_FLAG_SFTRST) != RESET)
  {
    return SYSTEM_HEALTH_RESET_SOFTWARE;
  }

  if (__HAL_RCC_GET_FLAG(RCC_FLAG_LPWRRST) != RESET)
  {
    return SYSTEM_HEALTH_RESET_LOW_POWER;
  }

  if (__HAL_RCC_GET_FLAG(RCC_FLAG_OBLRST) != RESET)
  {
    return SYSTEM_HEALTH_RESET_OPTION_BYTE;
  }

  if (__HAL_RCC_GET_FLAG(RCC_FLAG_FWRST) != RESET)
  {
    return SYSTEM_HEALTH_RESET_FIREWALL;
  }

  if (__HAL_RCC_GET_FLAG(RCC_FLAG_BORRST) != RESET)
  {
    return SYSTEM_HEALTH_RESET_BROWN_OUT;
  }

  if (__HAL_RCC_GET_FLAG(RCC_FLAG_PINRST) != RESET)
  {
    return SYSTEM_HEALTH_RESET_PIN;
  }

  return SYSTEM_HEALTH_RESET_POWER_ON;
}

static uint8_t StartIndependentWatchdog(void)
{
  uint32_t start_ms = HAL_GetTick();

  __HAL_RCC_LSI_ENABLE();
  while (__HAL_RCC_GET_FLAG(RCC_FLAG_LSIRDY) == RESET)
  {
    if ((HAL_GetTick() - start_ms) > SYSTEM_HEALTH_LSI_READY_TIMEOUT_MS)
    {
      return 0U;
    }
  }

  IWDG->KR = SYSTEM_HEALTH_IWDG_KEY_ENABLE;
  IWDG->KR = SYSTEM_HEALTH_IWDG_KEY_WRITE_ACCESS;
  IWDG->PR = SYSTEM_HEALTH_IWDG_PRESCALER_256;
  IWDG->RLR = SYSTEM_HEALTH_IWDG_RELOAD_MAX;
  IWDG->WINR = SYSTEM_HEALTH_IWDG_RELOAD_MAX;

  start_ms = HAL_GetTick();
  while (IWDG->SR != 0U)
  {
    if ((HAL_GetTick() - start_ms) > SYSTEM_HEALTH_LSI_READY_TIMEOUT_MS)
    {
      IWDG->KR = SYSTEM_HEALTH_IWDG_KEY_RELOAD;
      return 1U;
    }
  }

  IWDG->KR = SYSTEM_HEALTH_IWDG_KEY_RELOAD;

  return 1U;
}

void SystemHealth_Init(void)
{
  system_health_reset_reason = ReadResetReason();
  __HAL_RCC_CLEAR_RESET_FLAGS();
  system_health_watchdog_enabled = StartIndependentWatchdog();
}

void SystemHealth_WatchdogRefresh(void)
{
  if (system_health_watchdog_enabled != 0U)
  {
    IWDG->KR = SYSTEM_HEALTH_IWDG_KEY_RELOAD;
  }
}

void SystemHealth_RequestReset(void)
{
  NVIC_SystemReset();
}

uint8_t SystemHealth_IsWatchdogEnabled(void)
{
  return system_health_watchdog_enabled;
}

SystemHealthResetReason SystemHealth_GetResetReason(void)
{
  return system_health_reset_reason;
}

const char *SystemHealth_GetResetReasonText(void)
{
  switch (system_health_reset_reason)
  {
    case SYSTEM_HEALTH_RESET_POWER_ON:
      return "power_on";
    case SYSTEM_HEALTH_RESET_PIN:
      return "pin";
    case SYSTEM_HEALTH_RESET_BROWN_OUT:
      return "brown_out";
    case SYSTEM_HEALTH_RESET_SOFTWARE:
      return "software";
    case SYSTEM_HEALTH_RESET_IWDG:
      return "iwdg";
    case SYSTEM_HEALTH_RESET_WWDG:
      return "wwdg";
    case SYSTEM_HEALTH_RESET_LOW_POWER:
      return "low_power";
    case SYSTEM_HEALTH_RESET_OPTION_BYTE:
      return "option_byte";
    case SYSTEM_HEALTH_RESET_FIREWALL:
      return "firewall";
    default:
      return "unknown";
  }
}
