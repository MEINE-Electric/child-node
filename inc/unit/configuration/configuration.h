#pragma once

#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>

#include "drivers/load.h"
#include "drivers/supply.h"
#include "drivers/module.h"
#include "mqtt/mqtt.h"
#include "registry/registry.h"
#include "unit/channel/channel.h"
#include "event/event.h"

class UnitConfiguration
{
public:
    UnitConfiguration(std::string nodeID, MQTT& mqtt, Registry& registry);
    ~UnitConfiguration();

    void startEventWatchdog();
    void stopEventWatchdog();
    void startTelemetryThread();
    void stopTelemetryThread();
    void startReconnectionThread();
    void stopReconnectionThread();

    bool waitForConfiguration();
private:
    std::string nodeID;
    MQTT& mqtt;
    Registry& registry;
    ChannelEventBus eventBus;
    
    std::mutex mutex;
    std::condition_variable cv;
    bool configured = false;

    std::unordered_map<std::string, Channel> channelList;
    std::unordered_map<std::string, Load> loadList;
    std::unordered_map<std::string, Supply> supplyList;
    std::unordered_map<std::string, Module> moduleList;

    std::jthread eventWatchdog;
    std::jthread telemetryThread;
    std::jthread reconnectionThread;
    std::mutex channelListMutex;

    void checkForEvents();
    void handleConfig(const std::string& message);
    void handleExperiment(const std::string& message);
    void handleControl(const std::string& message);
    void publishFeedback(const std::string& section, const std::string& message);

    template <typename T> 
    void reconnectDevice(std::unordered_map<std::string, T>& devices, const ChannelEvent& event);
};