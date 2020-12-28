#ifndef EspHomeClient_h
#define EspHomeClient_h

#include <PubSubClient.h>
#include <vector>

#ifdef ESP8266

#include <ESP8266WiFi.h>

#else //ESP32

#include <WiFiClient.h>

#endif

void onConnectionEstablished();

typedef std::function<void()> ConnectionEstablishedCallback;
typedef std::function<void(const String &message)> MessageReceivedCallback;
typedef std::function<void(const String &topicStr, const String &message)> MessageReceivedCallbackWithTopic;

enum TopicType
{
    CMND,
    STAT,
    TELE
};

class EspHomeClient
{
private:
    // Wifi
    bool handle_wifi_;
    bool wifi_connected_;
    bool connecting_to_wifi_;
    unsigned long last_wifi_connection_attempt_millis_;
    unsigned long next_wifi_connection_attempt_millis_;
    unsigned int wifi_reconnection_attempt_delay_;
    const char *wifi_ssid_;
    const char *wifi_password_;
    WiFiClient wifi_client_;

    // MQTT
    bool mqtt_connected_;
    unsigned long next_mqtt_connection_attempt_millis_;
    unsigned int mqtt_reconnection_attempt_delay_;
    const char *mqtt_broker_;
    const short mqtt_port_;
    const char *mqtt_username_;
    const char *mqtt_password_;
    const char *mqtt_client_name_;
    bool mqtt_clean_session_;
    char *mqtt_last_will_topic_;
    char *mqtt_last_will_message_;
    bool mqtt_last_will_retain_;
    unsigned int failed_mqtt_connection_attempt_count_;
    PubSubClient mqtt_client_;

    struct TopicSubscriptionRecord
    {
        String topic;
        MessageReceivedCallback callback;
        MessageReceivedCallbackWithTopic callback_with_topic;
    };
    std::vector<TopicSubscriptionRecord> topic_subscription_list_;

    // Other
    ConnectionEstablishedCallback connection_established_callback_;
    bool enable_serial_logs_;
    bool drastic_reset_on_connection_failures_;
    unsigned int connection_established_count_;

public:
    EspHomeClient(
        const char *wifi_ssid,
        const char *wifi_password,
        const char *mqtt_broker,
        const char *mqtt_client_name = "ESP8266",
        const short mqtt_port = 1883);

    EspHomeClient(
        const char *wifi_ssid,
        const char *wifi_password,
        const char *mqtt_broker,
        const char *mqtt_username,
        const char *mqtt_password,
        const char *mqtt_client_name = "ESP8266",
        const short mqtt_port = 1883);

    // Configuration
    void enableDebuggingMessages(const bool enabled = true); // Allow to display useful debugging messages. Can be set to false to disable them during program execution
    void enableMQTTPersistence(); // Tell the broker to establish a persistent connection. Disabled by default. Must be called before the first loop() execution
    void enableLastWillMessage(const char* topic, const char* message, const bool retain = false); // Must be set before the first loop() call.
    void enableDrasticResetOnConnectionFailures() {drastic_reset_on_connection_failures_ = true;} // Can be usefull in special cases where the ESP board hang and need resetting (#59)

    // Main loop, to call at each sketch loop()
    void loop();

    // MQTT
    bool setMaxPacketSize(const uint16_t size); // Pubsubclient >= 2.8; override the default value of MQTT_MAX_PACKET_SIZE
    bool publish(TopicType type, const String &topic, const String &payload, bool retain = false);
    bool subscribe(const String &topic, MessageReceivedCallback message_received_callback, uint8_t qos = 0);
    bool subscribe(const String &topic, MessageReceivedCallbackWithTopic message_received_callback, uint8_t qos = 0);
    bool unsubscribe(const String &topic);
    void setKeepAlive(uint16_t keep_alive_seconds); // Change the keepalive interval (15 seconds by default)

    // Other
    inline bool isConnected() const { return isWifiConnected() && isMqttConnected(); };
    inline bool isWifiConnected() const { return wifi_connected_; };
    inline bool isMqttConnected() const { return mqtt_connected_; };

    // Default to onConnectionEstablished, you might want to override this for special cases like two MQTT connections in the same sketch
    inline void setOnConnectionEstablishedCallback(ConnectionEstablishedCallback callback) { connection_established_callback_ = callback; };

private:
    bool handleWifi();
    bool handleMQTT();
    void onWiFiConnectionEstablished();
    void onWiFiConnectionLost();
    void onMQTTConnectionEstablished();
    void onMQTTConnectionLost();

    void connectToWifi();
    bool connectToMqttBroker();
    bool mqttTopicMatch(const String &topic1, const String &topic2);
    void mqttMessageReceivedCallback(char *topic, byte *payload, unsigned int length);

    String buildFullTopic(TopicType type, const String &topic);
};

#endif