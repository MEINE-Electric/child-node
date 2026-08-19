#pragma once

#include <mqtt/async_client.h>
#include <chrono>
#include <optional>
#include <string>

class MQTT : public virtual mqtt::callback
{
public:
    MQTT(const std::string& broker, const std::string& clientId);
    ~MQTT();

    // Connection
    bool connect();
    void disconnect();
    bool isConnected() const;

    void publish(const std::string& topic,const std::string& message);
    void publishRetained(const std::string& topic,const std::string& message);

    void subscribe(const std::string& topic);
    void message_arrived(mqtt::const_message_ptr msg) override;

    // Event handling
    std::optional<std::pair<std::string, std::string>>  waitForEvent();

private:
    std::string broker;
    std::string clientId;
    bool connected;

    void reconnect();

    mqtt::async_client client;
    mqtt::connect_options connOpts;

    // Application event queue
    std::mutex eventMutex;
    std::condition_variable eventCV;
    std::queue<std::pair<std::string, std::string>> eventQueue;
};
