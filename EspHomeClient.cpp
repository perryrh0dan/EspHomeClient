#include "EspHomeClient.h"

// Wifi and MQTT handling
EspHomeClient::EspHomeClient(
  const char* wifi_ssid,
  const char* wifi_password,
  const char* mqtt_broker,
  const char* mqtt_client_name,
  const short mqtt_port) :
  EspHomeClient(wifi_ssid, wifi_password, mqtt_broker, NULL, NULL, mqtt_client_name, mqtt_port){}

EspHomeClient::EspHomeClient(
    const char *wifi_ssid,
    const char *wifi_password,
    const char *mqtt_broker,
    const char *mqtt_username,
    const char *mqtt_password,
    const char *mqtt_client_name,
    const short mqtt_port) : wifi_ssid_(wifi_ssid),
                             wifi_password_(wifi_password),
                             mqtt_broker_(mqtt_broker),
                             mqtt_username_(mqtt_username),
                             mqtt_password_(mqtt_password),
                             mqtt_client_name_(mqtt_client_name),
                             mqtt_port_(mqtt_port),
                             mqtt_client_(mqtt_broker, mqtt_port, wifi_client_)
{
  // WiFi connection
  handle_wifi_ = (wifi_ssid_ != NULL);
  wifi_connected_ = false;
  connecting_to_wifi_ = false;
  next_wifi_connection_attempt_millis_ = 500;
  last_wifi_connection_attempt_millis_ = 0;
  wifi_reconnection_attempt_delay_ = 60 * 1000;

  // MQTT client
  mqtt_connected_ = false;
  next_mqtt_connection_attempt_millis_ = 0;
  mqtt_reconnection_attempt_delay_ = 15 * 1000; // 15 seconds of waiting between each mqtt reconnection attempts by default
  mqtt_last_will_topic_ = 0;
  mqtt_last_will_message_ = 0;
  mqtt_last_will_retain_ = false;
  mqtt_clean_session_ = true;
  mqtt_client_.setCallback([this](char *topic, byte *payload, unsigned int length) { this->mqttMessageReceivedCallback(topic, payload, length); });
  failed_mqtt_connection_attempt_count_ = 0;

  // other
  connection_established_callback_ = onConnectionEstablished;
  enable_serial_logs_ = false;
  drastic_reset_on_connection_failures_ = false;
  connection_established_count_ = 0;
}

// ##### Configuration ####

void EspHomeClient::enableDebuggingMessages(const bool enabled)
{
  enable_serial_logs_ = enabled;
}

void EspHomeClient::enableMQTTPersistence()
{
  mqtt_clean_session_ = false;
}

void EspHomeClient::enableLastWillMessage(const char* topic, const char* message, const bool retain)
{
  mqtt_last_will_topic_ = (char*)topic;
  mqtt_last_will_message_ = (char*)message;
  mqtt_last_will_retain_ = retain;
}

// ##### Main Loop ####

void EspHomeClient::loop()
{
  // WIFI handling
  bool wifi_state_changed_ = handleWifi();

  // If there is a change in the wifi connection state, don't handle the mqtt connection state right away.
  // We will wait at least one lopp() call. This prevent the library from doing too much thing in the same loop() call.
  if (wifi_state_changed_)
    return;

  // MQTT Handling
  bool mqtt_state_changed_ = handleMQTT();
  if (mqtt_state_changed_)
    return;
}

// ##### Public Functions ####

bool EspHomeClient::setMaxPacketSize(const uint16_t size)
{

  bool success = mqtt_client_.setBufferSize(size);

  if(!success && enable_serial_logs_)
    Serial.println("MQTT! failed to set the max packet size.");

  return success;
}

bool EspHomeClient::publish(TopicType type, const String &topic, const String &payload, bool retain)
{
  // Do not try to publish if MQTT is not connected.
  if (!isConnected())
  {
    if (enable_serial_logs_)
      Serial.println("MQTT! Trying to publish when disconnected, skipping.");

    return false;
  }

  // Build full topic
  String full_topic = buildFullTopic(type, topic);

  bool success = mqtt_client_.publish(full_topic.c_str(), payload.c_str(), retain);

  if (enable_serial_logs_)
  {
    if (success)
      Serial.printf("MQTT << [%s] %s\n", full_topic.c_str(), payload.c_str());
    else
      Serial.println("MQTT! publish failed, is the message too long ? (see setMaxPacketSize())"); // This can occurs if the message is too long according to the maximum defined in PubsubClient.h
  }

  return success;
}

bool EspHomeClient::subscribe(const String &topic, MessageReceivedCallback message_received_callback, uint8_t qos)
{
  // Do not try to subscribe if MQTT is not connected.
  if (!isConnected())
  {
    if (enable_serial_logs_)
      Serial.println("MQTT! Trying to subscribe when disconnected, skipping.");

    return false;
  }

  // Build full topic
  String full_topic = buildFullTopic(CMND, topic);

  bool success = mqtt_client_.subscribe(full_topic.c_str(), qos);

  if (success)
  {
    // Add the record to the subscription list only if it does not exists.
    bool found = false;
    for (byte i = 0; i < topic_subscription_list_.size() && !found; i++)
      found = topic_subscription_list_[i].topic.equals(full_topic);

    if (!found)
      topic_subscription_list_.push_back({full_topic, message_received_callback, NULL});
  }

  if (enable_serial_logs_)
  {
    if (success)
      Serial.printf("MQTT: Subscribed to [%s]\n", full_topic.c_str());
    else
      Serial.println("MQTT! subscribe failed");
  }

  return success;
}

bool EspHomeClient::subscribe(const String &topic, MessageReceivedCallbackWithTopic message_received_callback, uint8_t qos)
{
  if (subscribe(topic, (MessageReceivedCallback)NULL, qos))
  {
    topic_subscription_list_[topic_subscription_list_.size() - 1].callback_with_topic = message_received_callback;
    return true;
  }
  return false;
}

bool EspHomeClient::unsubscribe(const String &topic)
{
  // Do not try to unsubscribe if MQTT is not connected.
  if (!isConnected())
  {
    if (enable_serial_logs_)
      Serial.println("MQTT! Trying to unsubscribe when disconnected, skipping.");

    return false;
  }

  // Build full topic
  String full_topic = buildFullTopic(CMND, topic);

  for (int i = 0; i < topic_subscription_list_.size(); i++)
  {
    if (topic_subscription_list_[i].topic.equals(full_topic))
    {
      if (mqtt_client_.unsubscribe(full_topic.c_str()))
      {
        topic_subscription_list_.erase(topic_subscription_list_.begin() + i);
        i--;

        if (enable_serial_logs_)
          Serial.printf("MQTT: Unsubscribed from %s\n", full_topic.c_str());
      }
      else
      {
        if (enable_serial_logs_)
          Serial.println("MQTT! unsubscribe failed");

        return false;
      }
    }
  }

  return true;
}

void EspHomeClient::setKeepAlive(uint16_t keep_alive_seconds)
{
  mqtt_client_.setKeepAlive(keep_alive_seconds);
}

// ##### Private Function ####

bool EspHomeClient::handleWifi()
{
  // When it's the first call, reset the wifi radio and schedule the wifi connection
  static bool first_loop_call = true;
  if (handle_wifi_ && first_loop_call)
  {
    WiFi.disconnect(true);
    next_wifi_connection_attempt_millis_ = millis() + 500;
    first_loop_call = false;
    return true;
  }

  // Get the current connextion status
  bool is_wifi_connected = (WiFi.status() == WL_CONNECTED);

  /***** Detect ans handle the current WiFi handling state *****/

  // Connection established
  if (is_wifi_connected && !wifi_connected_)
  {
    onWiFiConnectionEstablished();
    connecting_to_wifi_ = false;

    // At least 500 miliseconds of waiting before an mqtt connection attempt.
    // Some people have reported instabilities when trying to connect to
    // the mqtt broker right after being connected to wifi.
    // This delay prevent these instabilities.
    next_mqtt_connection_attempt_millis_ = millis() + 500;
  }

  // Connection in progress
  else if (connecting_to_wifi_)
  {
    if (WiFi.status() == WL_CONNECT_FAILED || millis() - last_wifi_connection_attempt_millis_ >= wifi_reconnection_attempt_delay_)
    {
      if (enable_serial_logs_)
        Serial.printf("WiFi! Connection attempt failed, delay expired. (%fs). \n", millis() / 1000.0);

      WiFi.disconnect(true);

      next_wifi_connection_attempt_millis_ = millis() + 500;
      connecting_to_wifi_ = false;
    }
  }

  // Connection lost
  else if (!is_wifi_connected && wifi_connected_)
  {
    onWiFiConnectionLost();

    if (handle_wifi_)
      next_wifi_connection_attempt_millis_ = millis() + 500;
  }

  // Disconnected since at least one loop() call
  // Then, if we handle the wifi reconnection process and the waiting delay has expired, we connect to wifi
  else if (handle_wifi_ && next_wifi_connection_attempt_millis_ > 0 && millis() >= next_wifi_connection_attempt_millis_)
  {
    connectToWifi();
    next_wifi_connection_attempt_millis_ = 0;
    connecting_to_wifi_ = true;
    last_wifi_connection_attempt_millis_ = millis();
  }

  /**** Detect and return if there was a change in the WiFi state ****/

  if (is_wifi_connected != wifi_connected_)
  {
    wifi_connected_ = is_wifi_connected;
    return true;
  }
  else
    return false;
}

bool EspHomeClient::handleMQTT()
{
  // PubSubClient main lopp() call
  mqtt_client_.loop();

  // Get the current connextion status
  bool is_mqtt_connected = (isWifiConnected() && mqtt_client_.connected());

  /***** Detect ans handle the current MQTT handling state *****/

  // Connection established
  if (is_mqtt_connected && !mqtt_connected_)
  {
    mqtt_connected_ = true;
    onMQTTConnectionEstablished();
  }

  // Connection lost
  else if (!is_mqtt_connected && mqtt_connected_)
  {
    onMQTTConnectionLost();
    next_mqtt_connection_attempt_millis_ = millis() + mqtt_reconnection_attempt_delay_;
  }

  // It's time to  connect to the MQTT broker
  else if (isWifiConnected() && next_mqtt_connection_attempt_millis_ > 0 && millis() >= next_mqtt_connection_attempt_millis_)
  {
    // Connect to MQTT broker
    if (connectToMqttBroker())
    {
      failed_mqtt_connection_attempt_count_ = 0;
      next_mqtt_connection_attempt_millis_ = 0;
    }
    else
    {
      // Connection failed, plan another connection attempt
      next_mqtt_connection_attempt_millis_ = millis() + mqtt_reconnection_attempt_delay_;
      mqtt_client_.disconnect();
      failed_mqtt_connection_attempt_count_++;

      if (enable_serial_logs_)
        Serial.printf("MQTT!: Failed MQTT connection count: %i \n", failed_mqtt_connection_attempt_count_);

      // When there is too many failed attempt, sometimes it help to reset the WiFi connection or to restart the board.
      if (failed_mqtt_connection_attempt_count_ == 8)
      {
        if (enable_serial_logs_)
          Serial.println("MQTT!: Can't connect to broker after too many attempt, resetting WiFi ...");

        WiFi.disconnect(true);
        next_wifi_connection_attempt_millis_ = millis() + 500;

        if (!drastic_reset_on_connection_failures_)
          failed_mqtt_connection_attempt_count_ = 0;
      }
      else if (drastic_reset_on_connection_failures_ && failed_mqtt_connection_attempt_count_ == 12) // Will reset after 12 failed attempt (3 minutes of retry)
      {
        if (enable_serial_logs_)
          Serial.println("MQTT!: Can't connect to broker after too many attempt, resetting board ...");

#ifdef ESP8266
        ESP.reset();
#else
        ESP.restart();
#endif
      }
    }
  }

  /**** Detect and return if there was a change in the MQTT state ****/
  if (mqtt_connected_ != is_mqtt_connected)
  {
    mqtt_connected_ = is_mqtt_connected;
    return true;
  }
  else
    return false;
}

// Initiate a Wifi connection (non-blocking)
void EspHomeClient::connectToWifi()
{
  WiFi.mode(WIFI_STA);
#ifdef ESP32
  WiFi.setHostname(mqtt_client_name_);
#else
  WiFi.hostname(mqtt_client_name_);
#endif
  WiFi.begin(wifi_ssid_, wifi_password_);

  if (enable_serial_logs_)
    Serial.printf("\nWiFi: Connecting to %s ... (%fs) \n", wifi_ssid_, millis() / 1000.0);
}

// Try to connect to the MQTT broker and return True if the connection is successfull (blocking)
bool EspHomeClient::connectToMqttBroker()
{
  if (enable_serial_logs_)
    Serial.printf("MQTT: Connecting to broker \"%s\" with client name \"%s\" ... (%fs)", mqtt_broker_, mqtt_client_name_, millis() / 1000.0);

  bool success = mqtt_client_.connect(mqtt_client_name_, mqtt_username_, mqtt_password_, mqtt_last_will_topic_, 0, mqtt_last_will_retain_, mqtt_last_will_message_, mqtt_clean_session_);

  if (enable_serial_logs_)
  {
    if (success)
      Serial.printf(" - ok. (%fs) \n", millis() / 1000.0);
    else
    {
      Serial.printf("unable to connect (%fs), reason: ", millis() / 1000.0);

      switch (mqtt_client_.state())
      {
      case -4:
        Serial.println("MQTT_CONNECTION_TIMEOUT");
        break;
      case -3:
        Serial.println("MQTT_CONNECTION_LOST");
        break;
      case -2:
        Serial.println("MQTT_CONNECT_FAILED");
        break;
      case -1:
        Serial.println("MQTT_DISCONNECTED");
        break;
      case 1:
        Serial.println("MQTT_CONNECT_BAD_PROTOCOL");
        break;
      case 2:
        Serial.println("MQTT_CONNECT_BAD_CLIENT_ID");
        break;
      case 3:
        Serial.println("MQTT_CONNECT_UNAVAILABLE");
        break;
      case 4:
        Serial.println("MQTT_CONNECT_BAD_CREDENTIALS");
        break;
      case 5:
        Serial.println("MQTT_CONNECT_UNAUTHORIZED");
        break;
      }

      Serial.printf("MQTT: Retrying to connect in %i seconds.\n", mqtt_reconnection_attempt_delay_ / 1000);
    }
  }

  return success;
}

void EspHomeClient::onWiFiConnectionEstablished()
{
  if (enable_serial_logs_)
    Serial.printf("WiFi: Connected (%fs), ip : %s \n", millis() / 1000.0, WiFi.localIP().toString().c_str());
}

void EspHomeClient::onWiFiConnectionLost()
{
  if (enable_serial_logs_)
    Serial.printf("WiFi! Lost connection (%fs). \n", millis() / 1000.0);

  // If we handle wifi, we force disconnection to clear the last connection
  if (handle_wifi_)
  {
    WiFi.disconnect(true);
  }
}

void EspHomeClient::onMQTTConnectionEstablished()
{
  connection_established_count_++;
  connection_established_callback_();
}

void EspHomeClient::onMQTTConnectionLost()
{
  if (enable_serial_logs_)
  {
    Serial.printf("MQTT! Lost connection (%fs). \n", millis() / 1000.0);
    Serial.printf("MQTT: Retrying to connect in %i seconds. \n", mqtt_reconnection_attempt_delay_ / 1000);
  }
}

/**
 * Matching MQTT topics, handling the eventual presence of a single wildcard character
 *
 * @param topic1 is the topic may contain a wildcard
 * @param topic2 must not contain wildcards
 * @return true on MQTT topic match, false otherwise
 */
bool EspHomeClient::mqttTopicMatch(const String &topic1, const String &topic2)
{
  int i = 0;

  if ((i = topic1.indexOf('#')) >= 0)
  {
    String t1a = topic1.substring(0, i);
    String t1b = topic1.substring(i + 1);
    if ((t1a.length() == 0 || topic2.startsWith(t1a)) &&
        (t1b.length() == 0 || topic2.endsWith(t1b)))
      return true;
  }
  else if ((i = topic1.indexOf('+')) >= 0)
  {
    String t1a = topic1.substring(0, i);
    String t1b = topic1.substring(i + 1);

    if ((t1a.length() == 0 || topic2.startsWith(t1a)) &&
        (t1b.length() == 0 || topic2.endsWith(t1b)))
    {
      if (topic2.substring(t1a.length(), topic2.length() - t1b.length()).indexOf('/') == -1)
        return true;
    }
  }
  else
  {
    return topic1.equals(topic2);
  }

  return false;
}

void EspHomeClient::mqttMessageReceivedCallback(char *topic, byte *payload, unsigned int length)
{
  // Convert the payload into a String
  // First, We ensure that we dont bypass the maximum size of the PubSubClient library buffer that originated the payload
  // This buffer has a maximum length of _mqttClient.getBufferSize() and the payload begin at "headerSize + topicLength + 1"
  unsigned int str_termination_pos;
  if (strlen(topic) + length + 9 >= mqtt_client_.getBufferSize())
  {
    str_termination_pos = length - 1;

    if (enable_serial_logs_)
      Serial.print("MQTT! Your message may be truncated, please set setMaxPacketSize() to a higher value.\n");
  }
  else
    str_termination_pos = length;

  // Second, we add the string termination code at the end of the payload and we convert it to a String object
  payload[str_termination_pos] = '\0';
  String payloadStr((char *)payload);
  String topicStr(topic);

  // Logging
  if (enable_serial_logs_)
    Serial.printf("MQTT >> [%s] %s\n", topic, payloadStr.c_str());

  // Send the message to subscribers
  for (byte i = 0; i < topic_subscription_list_.size(); i++)
  {
    if (mqttTopicMatch(topic_subscription_list_[i].topic, String(topic)))
    {
      if (topic_subscription_list_[i].callback != NULL)
        topic_subscription_list_[i].callback(payloadStr); // Call the callback
      if (topic_subscription_list_[i].callback_with_topic != NULL)
        topic_subscription_list_[i].callback_with_topic(topicStr, payloadStr); // Call the callback
    }
  }
}

String EspHomeClient::buildFullTopic(TopicType type, const String &topic)
{
  String prefix;
  switch (type)
  {
  case TopicType::CMND:
    prefix = "cmnd/";
    break;
  case TopicType::STAT:
    prefix = "stat/";
    break;
  case TopicType::TELE:
    prefix = "tele/";
    break;
  }

  String full_topic = prefix + String(mqtt_client_name_) + String("/") + topic;
  return full_topic;
}