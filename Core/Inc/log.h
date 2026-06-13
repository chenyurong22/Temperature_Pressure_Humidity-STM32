#ifndef LOG_H
#define LOG_H

#include "stm32l4xx_hal.h"

void Log_Init(UART_HandleTypeDef *uart);
void Log_Info(const char *format, ...);
void Log_Warn(const char *format, ...);
void Log_Error(const char *format, ...);

#endif
