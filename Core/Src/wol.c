#include "wol.h"

#include "app_config.h"
#include "log.h"
#include "wifi_service.h"

#include <string.h>

#define WOL_MAC_SIZE 6U
#define WOL_MAGIC_PACKET_SIZE 102U

static uint8_t IsMacConfigured(const uint8_t mac[WOL_MAC_SIZE])
{
  uint8_t index;

  for (index = 0U; index < WOL_MAC_SIZE; index++)
  {
    if (mac[index] != 0U)
    {
      return 1U;
    }
  }

  return 0U;
}

static void BuildMagicPacket(uint8_t packet[WOL_MAGIC_PACKET_SIZE], const uint8_t mac[WOL_MAC_SIZE])
{
  uint8_t index;

  memset(packet, 0xFF, WOL_MAC_SIZE);

  for (index = 0U; index < 16U; index++)
  {
    memcpy(&packet[WOL_MAC_SIZE + (index * WOL_MAC_SIZE)], mac, WOL_MAC_SIZE);
  }
}

WolStatus Wol_SendMagicPacket(const uint8_t local_ip[4])
{
  const uint8_t pc_mac[WOL_MAC_SIZE] = APP_WOL_PC_MAC;
  uint8_t broadcast_ip[4];
  uint8_t packet[WOL_MAGIC_PACKET_SIZE];
  uint16_t sent_length = 0U;
  WolStatus status = WOL_STATUS_ERROR;

  if ((local_ip == 0) || (IsMacConfigured(pc_mac) == 0U))
  {
    Log_Error("WoL target MAC address is not configured");
    return WOL_STATUS_ERROR;
  }

  broadcast_ip[0] = local_ip[0];
  broadcast_ip[1] = local_ip[1];
  broadcast_ip[2] = local_ip[2];
  broadcast_ip[3] = 255U;

  BuildMagicPacket(packet, pc_mac);

  Log_Info("Sending WoL magic packet to %u.%u.%u.%u:%u",
           broadcast_ip[0],
           broadcast_ip[1],
           broadcast_ip[2],
           broadcast_ip[3],
           APP_WOL_BROADCAST_PORT);

  if (WifiService_OpenUdpClient(APP_WOL_SOCKET_ID, "wol-broadcast", broadcast_ip, APP_WOL_BROADCAST_PORT, APP_WOL_SOCKET_LOCAL_PORT) != WIFI_SERVICE_STATUS_OK)
  {
    Log_Error("Unable to open WoL UDP socket");
    return WOL_STATUS_ERROR;
  }

  if (WifiService_Send(APP_WOL_SOCKET_ID, packet, sizeof(packet), &sent_length, APP_NETWORK_TIMEOUT_MS) == WIFI_SERVICE_STATUS_OK)
  {
    if (sent_length == sizeof(packet))
    {
      status = WOL_STATUS_OK;
      Log_Info("WoL magic packet sent successfully");
    }
    else
    {
      Log_Error("WoL packet was only partially sent (%u/%u bytes)", sent_length, (unsigned int)sizeof(packet));
    }
  }
  else
  {
    Log_Error("Unable to send WoL magic packet");
  }

  if (WifiService_CloseClient(APP_WOL_SOCKET_ID) != WIFI_SERVICE_STATUS_OK)
  {
    Log_Warn("Unable to close WoL UDP socket cleanly");
  }

  return status;
}
