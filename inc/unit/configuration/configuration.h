#pragma once

#include <string>
#include <thread>
#include <unordered_map>

#include "drivers/load.h"
#include "drivers/supply.h"
#include "drivers/module.h"
#include "mqtt/mqtt.h"
#include "unit/channel/channel.h"

class UnitConfiguration
{
public:
    UnitConfiguration(std::string nodeID, MQTT& mqtt);
    ~UnitConfiguration();

    void startEventWatchdog();
    void stopEventWatchdog();
    void publishFeedback(const std::string& section, const std::string& message);
private:
    std::string nodeID;
    MQTT& mqtt;
    
    std::unordered_map<std::string, Channel> channelList;
    std::unordered_map<std::string, Load> loadList;
    std::unordered_map<std::string, Supply> supplyList;
    std::unordered_map<std::string, Module> moduleList;

    std::jthread eventWatchdog;

    void checkForEvents();
    void handleConfig(const std::string& message);
    void handleExperiment(const std::string& message);
    void handleControl(const std::string& message);
};
