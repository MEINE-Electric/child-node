#include <fmt/format.h>
#include <nlohmann/json.hpp>

#include "mqtt/mqtt.h"
#include "helper/logger/logger.h"
#include "unit/configuration/configuration.h"

#define NODE_ID "1"

enum InitStage {
    STARTING,
    MQTT_CONNECTED,
    AWAITING_CONFIGURATION,
    CONFIGURATION_LOADED,
    READY,
    ERROR
};

void publishStatus(MQTT& mqtt, const std::string& nodeID, const std::string& message)
{
    nlohmann::json status = {
        {"nodeID", nodeID},
        {"status", }
    };

    mqtt.publishRetained(
        fmt::format("status/{}", nodeID),
        status.dump()
    );
}

int main()
{
    Logger::logger().init();

    // Initializing node
    InitStage stage = InitStage::STARTING;

    // MQTT
    MQTT mqtt("127.0.0.1", NODE_ID);
    mqtt.connect();
    stage = InitStage::MQTT_CONNECTED;

    mqtt.subscribe(fmt::format("config/{}", NODE_ID));
    mqtt.subscribe(fmt::format("experiment/{}", NODE_ID));
    mqtt.subscribe(fmt::format("control/{}", NODE_ID));
    stage = InitStage::AWAITING_CONFIGURATION;

    // Configuring node
    UnitConfiguration unit(NODE_ID, mqtt);
    unit.startEventWatchdog();
    unit.startTelemetryThread();

    std::cin.get();
}