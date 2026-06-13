#include "wifi_service.h"

#include "app_config.h"
#include "wifi.h"

WifiServiceStatus WifiService_Init(void)
{
  return (WIFI_Init() == WIFI_STATUS_OK) ? WIFI_SERVICE_STATUS_OK : WIFI_SERVICE_STATUS_ERROR;
}

WifiServiceStatus WifiService_Connect(void)
{
  return (WIFI_Connect(APP_WIFI_SSID, APP_WIFI_PASSWORD, APP_WIFI_SECURITY) == WIFI_STATUS_OK) ? WIFI_SERVICE_STATUS_OK : WIFI_SERVICE_STATUS_ERROR;
}

void WifiService_Disconnect(void)
{
  (void)WIFI_Disconnect();
}

WifiServiceStatus WifiService_GetLocalIp(uint8_t ip_addr[4])
{
  return (WIFI_GetIP_Address(ip_addr) == WIFI_STATUS_OK) ? WIFI_SERVICE_STATUS_OK : WIFI_SERVICE_STATUS_ERROR;
}

WifiServiceStatus WifiService_OpenClient(uint8_t socket_id, const uint8_t ip_addr[4], uint16_t remote_port, uint16_t local_port)
{
  return (WIFI_OpenClientConnection(socket_id, WIFI_TCP_PROTOCOL, APP_API_HOST, (uint8_t *)ip_addr, remote_port, local_port) == WIFI_STATUS_OK) ? WIFI_SERVICE_STATUS_OK : WIFI_SERVICE_STATUS_ERROR;
}

WifiServiceStatus WifiService_CloseClient(uint8_t socket_id)
{
  return (WIFI_CloseClientConnection(socket_id) == WIFI_STATUS_OK) ? WIFI_SERVICE_STATUS_OK : WIFI_SERVICE_STATUS_ERROR;
}

WifiServiceStatus WifiService_Send(uint8_t socket_id, const uint8_t *data, uint16_t length, uint16_t *sent_length, uint32_t timeout_ms)
{
  return (WIFI_SendData(socket_id, (uint8_t *)data, length, sent_length, timeout_ms) == WIFI_STATUS_OK) ? WIFI_SERVICE_STATUS_OK : WIFI_SERVICE_STATUS_ERROR;
}

WifiServiceStatus WifiService_Receive(uint8_t socket_id, uint8_t *data, uint16_t max_length, uint16_t *received_length, uint32_t timeout_ms)
{
  return (WIFI_ReceiveData(socket_id, data, max_length, received_length, timeout_ms) == WIFI_STATUS_OK) ? WIFI_SERVICE_STATUS_OK : WIFI_SERVICE_STATUS_ERROR;
}
