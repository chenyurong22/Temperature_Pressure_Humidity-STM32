#ifndef WIFI_SERVICE_H
#define WIFI_SERVICE_H

#include <stdint.h>

typedef enum
{
  WIFI_SERVICE_STATUS_OK = 0,
  WIFI_SERVICE_STATUS_ERROR
} WifiServiceStatus;

WifiServiceStatus WifiService_Init(void);
WifiServiceStatus WifiService_ResetModule(void);
WifiServiceStatus WifiService_Connect(void);
void WifiService_Disconnect(void);
WifiServiceStatus WifiService_GetLocalIp(uint8_t ip_addr[4]);
WifiServiceStatus WifiService_OpenTcpClient(uint8_t socket_id, const char *host, const uint8_t ip_addr[4], uint16_t remote_port, uint16_t local_port);
WifiServiceStatus WifiService_OpenUdpClient(uint8_t socket_id, const char *host, const uint8_t ip_addr[4], uint16_t remote_port, uint16_t local_port);
WifiServiceStatus WifiService_CloseClient(uint8_t socket_id);
WifiServiceStatus WifiService_Send(uint8_t socket_id, const uint8_t *data, uint16_t length, uint16_t *sent_length, uint32_t timeout_ms);
WifiServiceStatus WifiService_Receive(uint8_t socket_id, uint8_t *data, uint16_t max_length, uint16_t *received_length, uint32_t timeout_ms);

#endif
