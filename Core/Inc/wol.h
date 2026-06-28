#ifndef WOL_H
#define WOL_H

#include <stdint.h>

typedef enum
{
  WOL_STATUS_OK = 0,
  WOL_STATUS_ERROR
} WolStatus;

WolStatus Wol_SendMagicPacket(const uint8_t local_ip[4]);

#endif
