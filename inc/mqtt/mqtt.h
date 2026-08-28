#pragma once

#include <mqtt/async_client.h>

#include <condition_variable>
#include <thread>
#include <mutex>
#include <optional>
#include <queue>
#include <string>

/**
 * @brief MQTT client used for communication with the cycler system.
 *
 * Manages the MQTT connection, publishing, subscriptions, automatic
 * reconnection, and reception of application-level events.
 *
 * Incoming messages for the configured client are converted into
 * application events and stored in an internal thread-safe event queue.
 */
class MQTT : public virtual mqtt::callback
{
public:

    /**
     * @brief Constructs an MQTT client.
     *
     * Initializes the Paho MQTT asynchronous client and configures
     * the connection options.
     *
     * @param broker MQTT broker URI.
     * @param clientId Unique MQTT client identifier.
     */
    MQTT(const std::string& broker,
         const std::string& clientId);

    /**
     * @brief Disconnects from the MQTT broker and stops the
     * connection management thread.
     */
    ~MQTT();
    
    /**
    * @brief Connects to the MQTT broker.
    *
    * Attempts to establish a connection to the configured MQTT broker.
    *
    * @return true if the connection was established successfully;
    *         keeps retrying connection every 5 seconds.
    */
    bool connect();

    /**
     * @brief Disconnects from the MQTT broker.
     *
     * Requests a clean MQTT disconnection if the client is currently
     * connected.
     */
    void disconnect();

    /**
     * @brief Checks whether the MQTT client is currently connected.
     *
     * @return true if connected to the MQTT broker; false otherwise.
     */
    bool isConnected() const;

    /**
     * @brief Publishes a message to an MQTT topic.
     *
     * Publishes using QoS 1 without the MQTT retained flag.
     * If the client is disconnected, a reconnection attempt is made
     * before publishing.
     *
     * @param topic MQTT topic to publish to.
     * @param message Message payload.
     */
    void publish(const std::string& topic,
                 const std::string& message);

    /**
     * @brief Publishes a retained MQTT message.
     *
     * Publishes using QoS 1 with the MQTT retained flag enabled.
     * If the client is disconnected, a reconnection attempt is made
     * before publishing.
     *
     * @param topic MQTT topic to publish to.
     * @param message Message payload.
     */
    void publishRetained(const std::string& topic,
                         const std::string& message);

    /**
     * @brief Subscribes to an MQTT topic.
     *
     * The subscription uses QoS 1. If the client is disconnected,
     * a reconnection attempt is made before subscribing.
     *
     * @param topic MQTT topic to subscribe to.
     */
    void subscribe(const std::string& topic);

    /**
     * @brief Handles an incoming MQTT message.
     *
     * Called by the Paho MQTT client when a message is received.
     * Messages from recognized application topics are converted into
     * events and placed into the internal event queue.
     *
     * Recognized topics include:
     * - config/<clientId>
     * - experiment/<clientId>
     * - control/<clientId>
     *
     * @param msg Received MQTT message.
     */
    void message_arrived(mqtt::const_message_ptr msg) override;

    /**
     * @brief Retrieves the next pending application event.
     *
     * Waits for an event for up to 100 milliseconds. Events are removed
     * from the internal queue when returned.
     *
     * @return A pair containing the event type and message payload,
     *         or std::nullopt if no event is available before the timeout.
     */
    std::optional<std::pair<std::string, std::string>>
    waitForMQTTEvent();

private:

    /**
     * @brief Attempts to reconnect to the MQTT broker.
     *
     * Starts the connection process if the client is currently
     * disconnected.
     */
    void reconnect();

    /** @brief MQTT broker URI. */
    std::string broker;

    /** @brief Unique MQTT client identifier. */
    std::string clientId;

    /** @brief Current connection state. */
    bool connected;

    /** @brief Paho MQTT asynchronous client. */
    mqtt::async_client client;

    /** @brief MQTT connection configuration options. */
    mqtt::connect_options connOpts;

    /**
     * @brief Background thread responsible for maintaining the
     * MQTT connection.
     */
    std::jthread connectionThread;

    /** @brief Mutex protecting access to the application event queue. */
    std::mutex eventMutex;

    /** @brief Condition variable used to notify waiting consumers. */
    std::condition_variable eventCV;

    /**
     * @brief Queue of application events received through MQTT.
     *
     * Each event consists of an event type and its associated
     * message payload.
     */
    std::queue<std::pair<std::string, std::string>> eventQueue;
};