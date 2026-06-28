#ifndef MQTT_CLIENT_H
#define MQTT_CLIENT_H

#include <stdint.h>

typedef enum
{
  MQTT_CLIENT_STATUS_OK = 0,
  MQTT_CLIENT_STATUS_ERROR
} MqttClientStatus;

typedef void (*MqttClientWakeCallback)(void *user_context);

void MqttClient_Init(MqttClientWakeCallback wake_callback, void *user_context);
void MqttClient_Reset(void);
MqttClientStatus MqttClient_Process(void);
uint8_t MqttClient_IsConnected(void);

#endif
