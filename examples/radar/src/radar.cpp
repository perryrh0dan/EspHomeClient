#include "radar.h"

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
