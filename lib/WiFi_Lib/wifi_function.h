#ifndef _WIFI_FUNCTION_H_
#define _WIFI_FUNCTION_H_

bool wifi_setup_mqtt(void (*callback)(char* topic, byte* message, unsigned int length), char* ssid, char* password, char* mqtt_server, uint16_t port);

bool wifi_loop(uint8_t *DeviceID);

void wifi_disconnect(void);

bool wifi_mqtt_isConnected(void);

void wifi_mqtt_publish(uint8_t *DeviceID, String topic, char* dataBuffer);

#endif