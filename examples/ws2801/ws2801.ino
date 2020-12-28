#define FASTLED_ESP8266_RAW_PIN_ORDER
#include "FastLED.h"

#include<EspHomeClient.h>

// MQTT Topic
const char* topic = "office_led_1";

// WiFi & MQTT Server
const char* ssid = "";
const char* password = "";
const char* mqtt_broker = "";
const char* mqtt_username = "";
const char* mqtt_password = "";
const char* mqtt_client_name = "office_led_1";

// LED Config
#define NUM_LEDS 32
#define DATA_PIN 2
#define CLOCK_PIN 2

CRGB leds[NUM_LEDS];

EspHomeClient client(
  ssid, password, mqtt_broker, mqtt_username, mqtt_password, mqtt_client_name
);

void setup() {
  Serial.begin(115200);

  client.enableDebuggingMessages();

  // LEDs
  delay(2000);
  FastLED.addLeds<WS2801, DATA_PIN, CLOCK_PIN, RGB>(leds, NUM_LEDS);
}

void loop() {
  client.loop();
}

void onConnectionEstablished(){
  client.subscribe("power", onPowerCommand);
}

void onPowerCommand(const String &message) {
  if (message == "ON") {
    turn_on();
  } else if (message == "OFF") {
    turn_off();
  }
}

void turn_on() {
  for(int i = 0; i < NUM_LEDS; i++) {
    leds[i] = CRGB::Blue; 
  }
  FastLED.show();
}

void turn_off() {
  FastLED.clear();
  FastLED.show();
}
