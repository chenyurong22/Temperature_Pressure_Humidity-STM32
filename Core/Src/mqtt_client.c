#include "mqtt_client.h"

#include "app_config.h"
#include "log.h"
#include "wifi_service.h"

#include "stm32l4xx_hal.h"
#include <string.h>

#define MQTT_PACKET_BUFFER_SIZE 512U
#define MQTT_MAX_REMAINING_LENGTH_BYTES 4U
#define MQTT_CONNECT_FLAGS 0xC2U
#define MQTT_QOS1_FLAGS 0x02U

typedef enum
{
  MQTT_STATE_DISCONNECTED = 0,
  MQTT_STATE_CONNECTED
} MqttState;

static MqttState mqtt_state;
static uint32_t mqtt_next_retry_ms;
static uint32_t mqtt_last_io_ms;
static uint16_t mqtt_packet_id;
static MqttClientWakeCallback mqtt_wake_callback;
static void *mqtt_user_context;

static uint8_t TimeReached(uint32_t timestamp_ms)
{
  return ((int32_t)(HAL_GetTick() - timestamp_ms) >= 0) ? 1U : 0U;
}

static void ScheduleReconnect(void)
{
  mqtt_state = MQTT_STATE_DISCONNECTED;
  mqtt_next_retry_ms = HAL_GetTick() + APP_MQTT_RETRY_INTERVAL_MS;
  (void)WifiService_CloseClient(APP_MQTT_SOCKET_ID);
}

static uint16_t NextPacketId(void)
{
  mqtt_packet_id++;
  if (mqtt_packet_id == 0U)
  {
    mqtt_packet_id = 1U;
  }

  return mqtt_packet_id;
}

static uint8_t EncodeRemainingLength(uint8_t *buffer, uint32_t length, uint16_t *encoded_length)
{
  uint8_t index = 0U;

  do
  {
    uint8_t encoded_byte = (uint8_t)(length % 128U);
    length /= 128U;
    if (length > 0U)
    {
      encoded_byte |= 0x80U;
    }
    buffer[index++] = encoded_byte;
  } while ((length > 0U) && (index < MQTT_MAX_REMAINING_LENGTH_BYTES));

  if (length > 0U)
  {
    return 0U;
  }

  *encoded_length = index;
  return 1U;
}

static uint8_t WriteMqttString(uint8_t *buffer, uint16_t buffer_size, uint16_t *offset, const char *value)
{
  uint16_t length = (uint16_t)strlen(value);

  if (((uint32_t)*offset + 2U + length) > buffer_size)
  {
    return 0U;
  }

  buffer[*offset] = (uint8_t)(length >> 8);
  buffer[*offset + 1U] = (uint8_t)(length & 0xFFU);
  *offset += 2U;

  memcpy(&buffer[*offset], value, length);
  *offset += length;

  return 1U;
}

static MqttClientStatus SendPacket(const uint8_t *packet, uint16_t packet_length)
{
  uint16_t sent_length = 0U;

  if (WifiService_Send(APP_MQTT_SOCKET_ID, packet, packet_length, &sent_length, APP_NETWORK_TIMEOUT_MS) != WIFI_SERVICE_STATUS_OK)
  {
    return MQTT_CLIENT_STATUS_ERROR;
  }

  return (sent_length == packet_length) ? MQTT_CLIENT_STATUS_OK : MQTT_CLIENT_STATUS_ERROR;
}

static MqttClientStatus SendConnect(void)
{
  uint8_t packet[MQTT_PACKET_BUFFER_SIZE];
  uint8_t remaining_length_buffer[MQTT_MAX_REMAINING_LENGTH_BYTES];
  uint16_t remaining_length_size = 0U;
  uint16_t variable_offset = 0U;
  uint16_t packet_offset = 0U;

  if (WriteMqttString(&packet[5], sizeof(packet) - 5U, &variable_offset, "MQTT") == 0U)
  {
    return MQTT_CLIENT_STATUS_ERROR;
  }

  if (((uint32_t)variable_offset + 4U) > (sizeof(packet) - 5U))
  {
    return MQTT_CLIENT_STATUS_ERROR;
  }

  packet[5U + variable_offset++] = 4U;
  packet[5U + variable_offset++] = MQTT_CONNECT_FLAGS;
  packet[5U + variable_offset++] = (uint8_t)(APP_MQTT_KEEPALIVE_SECONDS >> 8);
  packet[5U + variable_offset++] = (uint8_t)(APP_MQTT_KEEPALIVE_SECONDS & 0xFFU);

  if ((WriteMqttString(&packet[5], sizeof(packet) - 5U, &variable_offset, APP_DEVICE_ID) == 0U) ||
      (WriteMqttString(&packet[5], sizeof(packet) - 5U, &variable_offset, APP_MQTT_USERNAME) == 0U) ||
      (WriteMqttString(&packet[5], sizeof(packet) - 5U, &variable_offset, APP_MQTT_PASSWORD) == 0U))
  {
    return MQTT_CLIENT_STATUS_ERROR;
  }

  if (EncodeRemainingLength(remaining_length_buffer, variable_offset, &remaining_length_size) == 0U)
  {
    return MQTT_CLIENT_STATUS_ERROR;
  }

  packet[packet_offset++] = 0x10U;
  memcpy(&packet[packet_offset], remaining_length_buffer, remaining_length_size);
  packet_offset += remaining_length_size;
  memmove(&packet[packet_offset], &packet[5], variable_offset);
  packet_offset += variable_offset;

  Log_Info("MQTT CONNECT to %s:%u", APP_MQTT_HOST, APP_MQTT_PORT);
  return SendPacket(packet, packet_offset);
}

static MqttClientStatus SendSubscribe(void)
{
  uint8_t packet[MQTT_PACKET_BUFFER_SIZE];
  uint8_t remaining_length_buffer[MQTT_MAX_REMAINING_LENGTH_BYTES];
  uint16_t remaining_length_size = 0U;
  uint16_t packet_offset = 0U;
  uint16_t variable_offset = 0U;
  uint16_t packet_id = NextPacketId();

  packet[5U + variable_offset++] = (uint8_t)(packet_id >> 8);
  packet[5U + variable_offset++] = (uint8_t)(packet_id & 0xFFU);

  if (WriteMqttString(&packet[5], sizeof(packet) - 5U, &variable_offset, APP_MQTT_WAKE_TOPIC) == 0U)
  {
    return MQTT_CLIENT_STATUS_ERROR;
  }

  if (((uint32_t)variable_offset + 1U) > (sizeof(packet) - 5U))
  {
    return MQTT_CLIENT_STATUS_ERROR;
  }

  packet[5U + variable_offset++] = 1U;

  if (EncodeRemainingLength(remaining_length_buffer, variable_offset, &remaining_length_size) == 0U)
  {
    return MQTT_CLIENT_STATUS_ERROR;
  }

  packet[packet_offset++] = 0x82U;
  memcpy(&packet[packet_offset], remaining_length_buffer, remaining_length_size);
  packet_offset += remaining_length_size;
  memmove(&packet[packet_offset], &packet[5], variable_offset);
  packet_offset += variable_offset;

  Log_Info("MQTT SUBSCRIBE %s", APP_MQTT_WAKE_TOPIC);
  return SendPacket(packet, packet_offset);
}

static MqttClientStatus SendPing(void)
{
  const uint8_t packet[2] = {0xC0U, 0x00U};

  Log_Info("MQTT PINGREQ");
  return SendPacket(packet, sizeof(packet));
}

static MqttClientStatus SendPubAck(uint16_t packet_id)
{
  uint8_t packet[4];

  packet[0] = 0x40U;
  packet[1] = 0x02U;
  packet[2] = (uint8_t)(packet_id >> 8);
  packet[3] = (uint8_t)(packet_id & 0xFFU);

  return SendPacket(packet, sizeof(packet));
}

static uint8_t ReadRemainingLength(const uint8_t *packet, uint16_t packet_length, uint16_t *offset, uint32_t *remaining_length)
{
  uint32_t multiplier = 1U;
  uint32_t value = 0U;
  uint8_t encoded_byte;
  uint8_t count = 0U;

  do
  {
    if (*offset >= packet_length)
    {
      return 0U;
    }

    encoded_byte = packet[(*offset)++];
    value += (uint32_t)(encoded_byte & 127U) * multiplier;
    multiplier *= 128U;
    count++;
  } while (((encoded_byte & 128U) != 0U) && (count < MQTT_MAX_REMAINING_LENGTH_BYTES));

  if ((encoded_byte & 128U) != 0U)
  {
    return 0U;
  }

  *remaining_length = value;
  return 1U;
}

static void HandlePublish(const uint8_t *packet, uint16_t packet_length, uint16_t offset, uint32_t remaining_length, uint8_t fixed_header)
{
  uint16_t end_offset = (uint16_t)(offset + remaining_length);
  uint16_t topic_length;
  uint16_t packet_id = 0U;
  uint8_t qos = (fixed_header >> 1) & 0x03U;
  const uint8_t *payload;
  uint16_t payload_length;

  if ((remaining_length > packet_length) || (end_offset > packet_length) || ((uint32_t)offset + 2U > packet_length))
  {
    ScheduleReconnect();
    return;
  }

  topic_length = (uint16_t)(((uint16_t)packet[offset] << 8) | packet[offset + 1U]);
  offset += 2U;

  if (((uint32_t)offset + topic_length) > end_offset)
  {
    ScheduleReconnect();
    return;
  }

  offset += topic_length;

  if (qos > 0U)
  {
    if (((uint32_t)offset + 2U) > end_offset)
    {
      ScheduleReconnect();
      return;
    }

    packet_id = (uint16_t)(((uint16_t)packet[offset] << 8) | packet[offset + 1U]);
    offset += 2U;
  }

  payload = &packet[offset];
  payload_length = (uint16_t)(end_offset - offset);

  if ((payload_length == 4U) && (memcmp(payload, "wake", 4U) == 0))
  {
    Log_Info("MQTT wake command received");
    if (mqtt_wake_callback != 0)
    {
      mqtt_wake_callback(mqtt_user_context);
    }
  }
  else
  {
    Log_Warn("MQTT ignored unsupported command (%u bytes)", payload_length);
  }

  if (qos == 1U)
  {
    if (SendPubAck(packet_id) != MQTT_CLIENT_STATUS_OK)
    {
      ScheduleReconnect();
    }
  }
}

static void HandleIncomingPacket(const uint8_t *packet, uint16_t packet_length)
{
  uint8_t packet_type;
  uint16_t offset = 1U;
  uint32_t remaining_length = 0U;

  if (packet_length < 2U)
  {
    return;
  }

  packet_type = packet[0] & 0xF0U;
  if (ReadRemainingLength(packet, packet_length, &offset, &remaining_length) == 0U)
  {
    ScheduleReconnect();
    return;
  }

  mqtt_last_io_ms = HAL_GetTick();

  if (packet_type == 0x30U)
  {
    HandlePublish(packet, packet_length, offset, remaining_length, packet[0]);
  }
  else if (packet_type == 0x90U)
  {
    Log_Info("MQTT SUBACK received");
  }
  else if (packet_type == 0xD0U)
  {
    Log_Info("MQTT PINGRESP received");
  }
}

static MqttClientStatus ReceiveExpected(uint8_t expected_packet_type)
{
  uint8_t packet[MQTT_PACKET_BUFFER_SIZE];
  uint16_t received_length = 0U;

  if (WifiService_Receive(APP_MQTT_SOCKET_ID, packet, sizeof(packet), &received_length, APP_NETWORK_TIMEOUT_MS) != WIFI_SERVICE_STATUS_OK)
  {
    return MQTT_CLIENT_STATUS_ERROR;
  }

  if ((received_length < 2U) || ((packet[0] & 0xF0U) != expected_packet_type))
  {
    return MQTT_CLIENT_STATUS_ERROR;
  }

  mqtt_last_io_ms = HAL_GetTick();
  return MQTT_CLIENT_STATUS_OK;
}

static MqttClientStatus ReceiveConnAck(void)
{
  uint8_t packet[MQTT_PACKET_BUFFER_SIZE];
  uint16_t received_length = 0U;

  if (WifiService_Receive(APP_MQTT_SOCKET_ID, packet, sizeof(packet), &received_length, APP_NETWORK_TIMEOUT_MS) != WIFI_SERVICE_STATUS_OK)
  {
    return MQTT_CLIENT_STATUS_ERROR;
  }

  if ((received_length < 4U) || ((packet[0] & 0xF0U) != 0x20U) || (packet[1] != 0x02U) || (packet[3] != 0x00U))
  {
    if (received_length >= 4U)
    {
      Log_Error("MQTT CONNACK rejected with code %u", packet[3]);
    }
    return MQTT_CLIENT_STATUS_ERROR;
  }

  mqtt_last_io_ms = HAL_GetTick();
  return MQTT_CLIENT_STATUS_OK;
}

static MqttClientStatus ConnectAndSubscribe(void)
{
  uint8_t mqtt_ip[4] = APP_MQTT_IP_ADDR;

  if (!TimeReached(mqtt_next_retry_ms))
  {
    return MQTT_CLIENT_STATUS_OK;
  }

  Log_Info("Opening MQTT TCP connection to %s:%u (%u.%u.%u.%u)",
           APP_MQTT_HOST,
           APP_MQTT_PORT,
           mqtt_ip[0],
           mqtt_ip[1],
           mqtt_ip[2],
           mqtt_ip[3]);

  if (WifiService_OpenTcpClient(APP_MQTT_SOCKET_ID, APP_MQTT_HOST, mqtt_ip, APP_MQTT_PORT, APP_MQTT_SOCKET_LOCAL_PORT) != WIFI_SERVICE_STATUS_OK)
  {
    Log_Error("Unable to open MQTT connection");
    ScheduleReconnect();
    return MQTT_CLIENT_STATUS_ERROR;
  }

  if ((SendConnect() != MQTT_CLIENT_STATUS_OK) || (ReceiveConnAck() != MQTT_CLIENT_STATUS_OK))
  {
    Log_Error("MQTT CONNECT failed");
    ScheduleReconnect();
    return MQTT_CLIENT_STATUS_ERROR;
  }

  if ((SendSubscribe() != MQTT_CLIENT_STATUS_OK) || (ReceiveExpected(0x90U) != MQTT_CLIENT_STATUS_OK))
  {
    Log_Error("MQTT SUBSCRIBE failed");
    ScheduleReconnect();
    return MQTT_CLIENT_STATUS_ERROR;
  }

  mqtt_state = MQTT_STATE_CONNECTED;
  mqtt_last_io_ms = HAL_GetTick();
  Log_Info("MQTT connected and subscribed");

  return MQTT_CLIENT_STATUS_OK;
}

void MqttClient_Init(MqttClientWakeCallback wake_callback, void *user_context)
{
  mqtt_state = MQTT_STATE_DISCONNECTED;
  mqtt_next_retry_ms = 0U;
  mqtt_last_io_ms = 0U;
  mqtt_packet_id = 0U;
  mqtt_wake_callback = wake_callback;
  mqtt_user_context = user_context;
}

void MqttClient_Reset(void)
{
  Log_Warn("MQTT client reset");
  ScheduleReconnect();
}

MqttClientStatus MqttClient_Process(void)
{
  uint8_t packet[MQTT_PACKET_BUFFER_SIZE];
  uint16_t received_length = 0U;

  if (mqtt_state != MQTT_STATE_CONNECTED)
  {
    return ConnectAndSubscribe();
  }

  if ((HAL_GetTick() - mqtt_last_io_ms) >= ((uint32_t)APP_MQTT_KEEPALIVE_SECONDS * 500UL))
  {
    if (SendPing() != MQTT_CLIENT_STATUS_OK)
    {
      Log_Error("MQTT PINGREQ failed");
      ScheduleReconnect();
      return MQTT_CLIENT_STATUS_ERROR;
    }
    mqtt_last_io_ms = HAL_GetTick();
  }

  if (WifiService_Receive(APP_MQTT_SOCKET_ID, packet, sizeof(packet), &received_length, APP_MQTT_POLL_TIMEOUT_MS) == WIFI_SERVICE_STATUS_OK)
  {
    if (received_length > 0U)
    {
      HandleIncomingPacket(packet, received_length);
    }
  }

  return MQTT_CLIENT_STATUS_OK;
}

uint8_t MqttClient_IsConnected(void)
{
  return (mqtt_state == MQTT_STATE_CONNECTED) ? 1U : 0U;
}
