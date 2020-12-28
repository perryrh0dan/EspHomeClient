#ifndef Radar_h
#define Radar_h

#include <EspHomeClient.h>

// WiFi & MQTT Server
const char* ssid = "";
const char* password = "";
const char* mqtt_broker = "";
const char* mqtt_username = "";
const char* mqtt_password = "";
const char* mqtt_client_name = "office_radar_1";

// Sensor
const int RADAR_SENSOR_PIN = 5;

EspHomeClient client(
  ssid, password, mqtt_broker, mqtt_username, mqtt_password, mqtt_client_name
);

void checkMovement();

#endif