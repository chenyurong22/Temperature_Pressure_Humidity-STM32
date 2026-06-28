#ifndef SYSTEM_HEALTH_H
#define SYSTEM_HEALTH_H

#include <stdint.h>

typedef enum
{
  SYSTEM_HEALTH_RESET_UNKNOWN = 0,
  SYSTEM_HEALTH_RESET_POWER_ON,
  SYSTEM_HEALTH_RESET_PIN,
  SYSTEM_HEALTH_RESET_BROWN_OUT,
  SYSTEM_HEALTH_RESET_SOFTWARE,
  SYSTEM_HEALTH_RESET_IWDG,
  SYSTEM_HEALTH_RESET_WWDG,
  SYSTEM_HEALTH_RESET_LOW_POWER,
  SYSTEM_HEALTH_RESET_OPTION_BYTE,
  SYSTEM_HEALTH_RESET_FIREWALL
} SystemHealthResetReason;

void SystemHealth_Init(void);
void SystemHealth_WatchdogRefresh(void);
void SystemHealth_RequestReset(void);
uint8_t SystemHealth_IsWatchdogEnabled(void);
SystemHealthResetReason SystemHealth_GetResetReason(void);
const char *SystemHealth_GetResetReasonText(void);

#endif
