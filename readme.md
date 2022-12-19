<h1 align="center">
  ESP8266/ESP32 Home Client
</h1>

<h4 align="center">
  ESP8266/ESP32 support library for WiFi and MQTT
</h4>

## Description

This library is intended to encapsulate the handling of WiFi and MQTT connections of an ESP8266/ESP32. Following thing will be handled by the library:

- Connecting to WiFi network
- Connecting to MQTT broker
- Three different mqtt topic types cmnd/stat/tele
- Automatically detecting connection lost either from the WiFi client or the MQTT broker and it will retry a connection automatically.
- Subscribing/unsubscrubing to/from MQTT topics by a friendly callback system.

## Contents

- [Description](#description)
- [Dependency](#dependency)
- [Install](#install)
- [Usage](#usage)
- [Subscribe & Publish](#subscribe--publish)
- [Development](#development)
- [Team](#team)
- [License](#license)

## Dependency

The MQTT communication depends on the PubSubClient Library (https://github.com/knolleary/pubsubclient).

## Install

Clone the repository and create a symbolic link inside of the arduino library directory:

### Windows

```cmd
mklink /D C:\Users\thomas\Documents\Arduino\libraries\PubSubClient D:\Dev\pubsubclient\
```

## Usage

```c++
#include "EspHomeClient.h"

EspHomeClient client(
  "WifiSSID",
  "WifiPassword",
  "MQTTBroker",     // MQTT Broker server ip
  "MQTTUsername",   // Optional
  "MQTTPassword",   // Optional
  "MQTTClient"      // Client name that uniquely identify your device is used for topic generation
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

## Subscribe & Publish

The MQTT format used by this library to subscribe and send messages contains of 3 parts:

`<prefix>/<topic>/<action>`

Beside subscribing to commands you can also publish message. The following types/prefix are supported.

### Prefix

| prefix | description                               |
| ------ | ----------------------------------------- |
| cmnd   | send a command                            |
| tele   | send telemetry data                       |
| stat   | send status information like power ON/OFF |

### Topic

The topic must be made unique by the user. It can be called `office` but it could also be called `office_light_1` as long as the user knows what it is and where to find it.

### Action

The action defines which command is to be executed on the target device, or in the case of status or telemetry messages, it defines the context of the message. E.g. `stat/office_light_1/power OFF`. Tells us that the light in the office is currently off.

## Development

Create a symbolic link to the 'EspHomeClient' library in your arduino libraries directory

```bash
ln -s /home/thomas/dev/esp8266/EspHomeClient /home/thomas/Arduino/libraries/
```

## Team

- Thomas Pöhlmann [(@perryrh0dan)](https://github.com/perryrh0dan)

## License

[MIT](https://github.com/perryrh0dan/esp8266/blob/master/license.md)
