#include <fmt/format.h>
#include <nlohmann/json.hpp>
#include <iostream>

#include "mqtt/mqtt.h"
#include "helper/logger/logger.h"
#include "registry/registry.h"
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

std::string stageToString(InitStage stage)
{
    switch (stage)
    {
        case InitStage::STARTING:
            return "STARTING";

        case InitStage::MQTT_CONNECTED:
            return "MQTT_CONNECTED";

        case InitStage::AWAITING_CONFIGURATION:
            return "AWAITING_CONFIGURATION";

        case InitStage::CONFIGURATION_LOADED:
            return "CONFIGURATION_LOADED";

        case InitStage::READY:
            return "READY";

        case InitStage::ERROR:
            return "ERROR";
    }

    return "UNKNOWN";
}

void setStage(InitStage& stage, InitStage newStage, MQTT& mqtt, const std::string& nodeID)
{
    stage = newStage;

    nlohmann::json status = {
        {"nodeID", nodeID},
        {"status", stageToString(stage)}
    };

    mqtt.publishRetained(
        fmt::format("status/{}", nodeID),
        status.dump()
    );
}

int main()
{
    Logger::logger().init();
    InitStage stage = InitStage::STARTING;

    MQTT mqtt("127.0.0.1", NODE_ID);

    mqtt.connect();

    setStage(stage, InitStage::MQTT_CONNECTED, mqtt, NODE_ID);

    Registry registry;
    registry.initialScan();
    registry.startWatchdog();

    nlohmann::json devices = nlohmann::json::array();

    for (const auto& [port, device] : registry.readDevices())
    {
        devices.push_back({
            {"idn", device.idn},
            {"port", device.port},
            {"baudrate", device.baudrate},
            {"available", device.available}
        });
    }

    mqtt.publish(
        fmt::format("devices/{}", NODE_ID),
        devices.dump()
    );
    
    mqtt.subscribe(fmt::format("config/{}", NODE_ID));
    mqtt.subscribe(fmt::format("experiment/{}", NODE_ID));
    mqtt.subscribe(fmt::format("control/{}", NODE_ID));

    setStage(stage, InitStage::AWAITING_CONFIGURATION, mqtt, NODE_ID);

    UnitConfiguration unit(NODE_ID, mqtt, registry);

    unit.startEventWatchdog();
    unit.waitForConfiguration();
    setStage(stage, InitStage::READY, mqtt, NODE_ID);

    unit.startTelemetryThread();

    std::cin.get();
}