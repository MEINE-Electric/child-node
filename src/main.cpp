#include <fmt/format.h>
#include <string>
#include <termios.h>
#include <nlohmann/json.hpp>

#include "helper/logger/logger.h"
#include "mqtt/mqtt.h"
#include "registry/registry.h"
#include "unit/configuration/configuration.h"

#define SETUP "1"

int main()
{
    Logger::logger().init();

    // Start MQTT
    MQTT mqtt("127.0.0.1",SETUP);
    mqtt.connect();
    // MQTT subscribe to config receival
    mqtt.subscribe(fmt::format("config/{}",SETUP));
    mqtt.subscribe(fmt::format("experiment/{}",SETUP));

    // Identify connected devices
    Registry registry = Registry();
    registry.initialScan();
    registry.startWatchdog();

    nlohmann::json devicesJson = nlohmann::json::array();
    for (const auto& [port, device] : registry.readDevices())
    {
        devicesJson.push_back({
            {"idn", device.idn},
            {"port", port},
            {"baudrate", static_cast<int>(device.baudrate)},
            {"available", device.available}
        });
    }

    mqtt.publishRetained(fmt::format("devices/{}", SETUP), devicesJson.dump());
    
    // Get configuration and experiments
    UnitConfiguration unit(SETUP, mqtt);
    unit.startEventWatchdog();

    std::cin.get();

    return 0;
}
