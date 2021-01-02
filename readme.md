<h1 align="center">
  ESP8266/ESP32 Home Client
</h1>

<h4 align="center">
  ESP8266/ESP32 support library for WiFi and MQTT
</h4>

## Contents

- [Dependency](#dependeny)
- [Install](#install)
- [Usage](#usage)
- [Development](#development)
- [Team](#team)
- [License](#license)

## Dependency

The MQTT communication depends on the PubSubClient Library (https://github.com/knolleary/pubsubclient).

## Install

## Usage

```c++
#include "EspHomeClient.h"

EspHomeClient client(
  "WifiSSID",
  "WifiPassword",
  "MQTTBroker",     // MQTT Broker server ip
  "MQTTUsername",   // Optional
  "MQTTPassword",   // Optional
  "MQTTClient"      // Client name that uniquely identify your device
);

void setup() {}

void onConnectionEstablished() {

  // Subscribe to "cmnd/<MQTTClient>/power"
  client.subscribe("power", [] (const String &payload)  {
    Serial.println(payload);
  });

  // Publish "01" to "tele/<MQTTClient>/present"
  client.publish(TELE, "present", "01");

  // Publish "ON" to "stat/<MQTTClient>/power"
  client.publish(STAT, "power", "ON");
}

void loop() {
  // Must be called at each loop() of your sketch
  client.loop();
}
```

## Development

Create a symbolic link to the 'EspHomeClient' library in your arduino libraries directory

``` bash
ln -s /home/thomas/dev/esp8266/EspHomeClient /home/thomas/Arduino/libraries/
```

## Team

- Thomas Pöhlmann [(@perryrh0dan)](https://github.com/perryrh0dan)

## License

[MIT](https://github.com/perryrh0dan/esp8266/blob/master/license.md)