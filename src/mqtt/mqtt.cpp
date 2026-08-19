#include <mqtt/message.h>
#include <nlohmann/json.hpp>

#include "mqtt/mqtt.h"
#include "helper/logger/logger.h"

MQTT::MQTT(const std::string& broker, const std::string& clientId)
    : broker(broker),
      clientId(clientId),
      client(broker, clientId),
      connected(false)
{
    connOpts.set_clean_session(true);
    client.set_callback(*this);
}

MQTT::~MQTT()
{
    disconnect();
}

bool MQTT::connect()
{
    try
    {
        client.connect(connOpts)->wait();
        connected = true;

        Logger::logger().log_mqtt()->info("Connected to {}", broker);
        return true;
    }
    catch (const mqtt::exception& e)
    {
        Logger::logger().log_mqtt()->error("Connect failed: {}", e.what());
        connected = false;
        return false;
    }
}

void MQTT::disconnect()
{
    if (!connected)
        return;

    try
    {
        client.disconnect()->wait();
        connected = false;
    }
    catch (const mqtt::exception& e)
    {
        Logger::logger().log_mqtt()->error("Disconnect failed: {}", e.what());
    }
}

bool MQTT::isConnected() const
{
    return connected;
}

void MQTT::publish(const std::string& topic,
                   const std::string& message)
{
    if (!connected)
        reconnect();

    try
    {
        constexpr int QOS = 1;
        constexpr bool RETAINED = false;

        mqtt::message_ptr msg = mqtt::make_message(topic, message);
        msg->set_qos(QOS);
        msg->set_retained(RETAINED);

        client.publish(msg)->wait();
    }
    catch (const mqtt::exception& e)
    {
        Logger::logger().log_mqtt()->error("Publish failed: {}", e.what());
        connected = false;
    }
}

void MQTT::publishRetained(const std::string& topic,
                   const std::string& message)
{
    if (!connected)
        reconnect();

    try
    {
        constexpr int QOS = 1;
        constexpr bool RETAINED = true;

        mqtt::message_ptr msg = mqtt::make_message(topic, message);
        msg->set_qos(QOS);
        msg->set_retained(RETAINED);

        client.publish(msg)->wait();
    }
    catch (const mqtt::exception& e)
    {
        Logger::logger().log_mqtt()->error("Publish failed: {}", e.what());
        connected = false;
    }
}

void MQTT::subscribe(const std::string& topic)
{
    if (!connected)
        reconnect();

    try
    {
        constexpr int QOS = 1;
        client.subscribe(topic, QOS)->wait();
    }
    catch (const mqtt::exception& e)
    {
        Logger::logger().log_mqtt()->error("Subscribe failed: {}", e.what());
        connected = false;
    }
}

void MQTT::reconnect()
{
    if (!connected)
        connect();
}

void MQTT::message_arrived(mqtt::const_message_ptr msg)
{
    Logger::logger().log_mqtt()->info("Received on {}: {}", msg->get_topic(), msg->to_string());

    if (msg->get_topic() == fmt::format("config/{}", clientId))
    {
        eventQueue.push(std::make_pair("config", msg->to_string()));
    }
    else if (msg->get_topic() == fmt::format("experiment/{}", clientId))
    {
        eventQueue.push(std::make_pair("experiment", msg->to_string()));
    }
    else if (msg->get_topic() == fmt::format("control/{}", clientId))
    {
        eventQueue.push(std::make_pair("control", msg->to_string()));
    }

    eventCV.notify_one();
}

std::optional<std::pair<std::string, std::string>> MQTT::waitForEvent()
{
    std::unique_lock lock(eventMutex);

    if (!eventCV.wait_for(lock, std::chrono::milliseconds(100), [this]()
    {
        return !eventQueue.empty();
    }))
    {
        return std::nullopt;
    }

    auto event = std::move(eventQueue.front());
    eventQueue.pop();

    return event;
}
