#include "log.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

static UART_HandleTypeDef *log_uart;

static void Log_Write(const char *level, const char *format, va_list args)
{
  char message[192];
  char line[224];
  int message_length;
  int line_length;

  if (log_uart == NULL)
  {
    return;
  }

  message_length = vsnprintf(message, sizeof(message), format, args);
  if (message_length < 0)
  {
    return;
  }

  line_length = snprintf(line, sizeof(line), "[%lu] %-5s %s\r\n", HAL_GetTick(), level, message);
  if (line_length < 0)
  {
    return;
  }

  if ((size_t)line_length >= sizeof(line))
  {
    line_length = (int)sizeof(line) - 1;
  }

  HAL_UART_Transmit(log_uart, (uint8_t *)line, (uint16_t)line_length, 100U);
}

void Log_Init(UART_HandleTypeDef *uart)
{
  log_uart = uart;
}

void Log_Info(const char *format, ...)
{
  va_list args;
  va_start(args, format);
  Log_Write("INFO", format, args);
  va_end(args);
}

void Log_Warn(const char *format, ...)
{
  va_list args;
  va_start(args, format);
  Log_Write("WARN", format, args);
  va_end(args);
}

void Log_Error(const char *format, ...)
{
  va_list args;
  va_start(args, format);
  Log_Write("ERROR", format, args);
  va_end(args);
}
