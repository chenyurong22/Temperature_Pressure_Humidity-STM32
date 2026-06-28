#ifndef APP_SECRETS_H
#define APP_SECRETS_H

#define APP_WIFI_SSID "your-2.4ghz-ssid"
#define APP_WIFI_PASSWORD "your-wifi-password"

#define APP_MQTT_HOST "your-mqtt-host.example.com"
#define APP_MQTT_IP_ADDR_VALUE_0 192U
#define APP_MQTT_IP_ADDR_VALUE_1 0U
#define APP_MQTT_IP_ADDR_VALUE_2 2U
#define APP_MQTT_IP_ADDR_VALUE_3 10U
#define APP_MQTT_IP_ADDR {192U, 0U, 2U, 10U}
#define APP_MQTT_PORT 1883U
#define APP_MQTT_USERNAME "your-mqtt-username"
#define APP_MQTT_PASSWORD "your-mqtt-password"
#define APP_MQTT_WAKE_TOPIC "your/private/wake/topic"
#define APP_MQTT_STATUS_TOPIC "your/private/status/topic"

#define APP_WOL_PC_MAC {0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U}

#endif
