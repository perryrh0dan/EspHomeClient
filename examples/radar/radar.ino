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

void setup() {
  Serial.begin(115200);

  client.enableDebuggingMessages();

  // Radar
  pinMode (RADAR_SENSOR_PIN, INPUT);
}

void loop() {
  client.loop();
  
  checkMovement();

  delay(2000);
}

void onConnectionEstablished(){}

void checkMovement() {
  int sensorValue = digitalRead(RADAR_SENSOR_PIN);
  
  if (sensorValue == 1) {
    char value[2];
    sprintf(value, "%02i", sensorValue);
    client.publish(TELE, "present", value);
  }
}
