#include <string>
#include <exception>
#include <unordered_set>

#include "mqtt/mqtt.h"
#include "helper/helper.h"
#include "helper/logger/logger.h"
#include "registry/registry.h"
#include "unit/configuration/configuration.h"
#include "event/event.h"

UnitConfiguration::UnitConfiguration(std::string nodeID, MQTT& mqtt, Registry& registry)
: nodeID(nodeID), mqtt(mqtt), registry(registry)
{
}

UnitConfiguration::~UnitConfiguration()
{
    stopEventWatchdog();
    stopTelemetryThread();
}

void UnitConfiguration::startEventWatchdog()
{
    if (eventWatchdog.joinable())
    {
        Logger::logger().log_unit()->warn("Event watchdog is already running");
        return;
    }

    eventWatchdog = std::jthread([this](std::stop_token stop)
    {
        Logger::logger().log_unit()->info("Event watchdog started");
        while (!stop.stop_requested())
        {
            checkForEvents();
        }
    });
}

void UnitConfiguration::stopEventWatchdog()
{
    if (eventWatchdog.joinable())
    {
        eventWatchdog.request_stop();
        eventWatchdog.join();
        Logger::logger().log_unit()->info("Event watchdog stopped");
    }
}

void UnitConfiguration::startTelemetryThread()
{
    if (telemetryThread.joinable())
    {
        Logger::logger().log_unit()->warn("Telemetry thread is already running");
        return;
    }

    telemetryThread = std::jthread([this](std::stop_token stop)
    {
        Logger::logger().log_unit()->info("Telemetry thread started");
        while (!stop.stop_requested())
        {
            std::vector<Channel::Data> telemetryData;

            {
                std::lock_guard lock(channelListMutex);

                for (auto& [id, channel] : channelList)
                {
                    if (channel.isInProcess())
                    {
                        telemetryData.push_back(channel.getLatestData());
                    }
                }
            }

            for (const auto& data : telemetryData)
            {
                mqtt.publish(
                    fmt::format("polling/{}/{}", nodeID, data.channelNumber),
                    data.toJson().dump()
                );
            }

            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    });
}

void UnitConfiguration::stopTelemetryThread()
{
    if (telemetryThread.joinable())
    {
        telemetryThread.request_stop();
        telemetryThread.join();
        Logger::logger().log_unit()->info("Telemetry Thread stopped");
    }  
}

bool UnitConfiguration::waitForConfiguration()
{
    std::unique_lock<std::mutex> lock(mutex);

    cv.wait(lock, [this]
    {
        return configured;
    });

    return true;
}

void UnitConfiguration::publishFeedback(const std::string& section, const std::string& message)
{
    mqtt.publish(fmt::format("{}/{}/feedback", section, nodeID), message);
}

void UnitConfiguration::checkForEvents()
{   
    // Capture events from MQTT
    const auto newMQTTEvent = mqtt.waitForMQTTEvent();
    const auto newChannelEvent = eventBus.waitForChannelEvent();

    if(!newMQTTEvent && !newChannelEvent)
    {
        return;
    }

    if(newMQTTEvent)
    {
        if (newMQTTEvent->first == "config")
        {
            handleConfig(newMQTTEvent->second);
        }
        else if (newMQTTEvent->first == "experiment")
        {
            handleExperiment(newMQTTEvent->second);
        }
        else if (newMQTTEvent->first == "control")
        {
            handleControl(newMQTTEvent->second);
        }
    }
    
    if (newChannelEvent)
    {
        if (newChannelEvent->type == ChannelEventType::LOAD_DISCONNECTED)
        {
            Logger::logger().log_unit()->warn("Load device (IDN: {}) disconnected from port {}, initiating reconnection for Channel {}", newChannelEvent->deviceIDN, newChannelEvent->devicePort, newChannelEvent->channelID);
            reconnectDevice(loadList, *newChannelEvent);
            Logger::logger().log_unit()->info("Load reconnection completed for Channel {}, clearing state and restarting", newChannelEvent->channelID);

        }

        else if (newChannelEvent->type == ChannelEventType::SUPPLY_DISCONNECTED)
        {
            Logger::logger().log_unit()->warn("Supply device (IDN: {}) disconnected from port {}, initiating reconnection for Channel {}", newChannelEvent->deviceIDN, newChannelEvent->devicePort, newChannelEvent->channelID);
            reconnectDevice(supplyList, *newChannelEvent);
            Logger::logger().log_unit()->info("Supply reconnection completed for Channel {}, clearing state and restarting", newChannelEvent->channelID);
        }

        else if (newChannelEvent->type == ChannelEventType::MODULE_DISCONNECTED)
        {
            Logger::logger().log_unit()->warn("Module device (IDN: {}) disconnected from port {}, initiating reconnection for Channel {}", newChannelEvent->deviceIDN, newChannelEvent->devicePort, newChannelEvent->channelID);
            reconnectDevice(moduleList, *newChannelEvent);
            Logger::logger().log_unit()->info("Module reconnection completed for Channel {}, clearing state and restarting", newChannelEvent->channelID);
        }

        channelList.at(newChannelEvent->channelID).clearState();
        channelList.at(newChannelEvent->channelID).enqueueCommand("start");
    }
}

template <typename T>
void UnitConfiguration::reconnectDevice(std::unordered_map<std::string, T>& devices, const ChannelEvent& event)
{
    Logger::logger().log_unit()->debug("Beginning reconnection process for device with IDN: {}", event.deviceIDN);
    
    auto node = devices.extract(event.devicePort);

    if (node.empty())
    {
        Logger::logger().log_unit()->error("Reconnect failed: device node not found at port {}", event.devicePort);
        devices.insert(std::move(node));
        Logger::logger().log_unit()->warn("Node is empty, cannot reconnect");
        return;
    }
    
    try
    {
        Logger::logger().log_unit()->debug("Disconnecting device (IDN: {}) from current port {}", event.deviceIDN, event.devicePort);
        node.mapped().disconnect();
        Logger::logger().log_unit()->debug("Device (IDN: {}) successfully disconnected from port {}", event.deviceIDN, event.devicePort);

        while (!node.mapped().isConnected())
        {
            Logger::logger().log_unit()->debug("Searching for device with IDN: {}", event.deviceIDN);
            std::string port = registry.findDeviceByIDN(event.deviceIDN);

            if (port.empty())
            {
                Logger::logger().log_unit()->warn("Device with IDN: {} not found in registry, waiting for device to appear", event.deviceIDN);
                registry.waitForDevice();
                Logger::logger().log_unit()->debug("Device scan completed, retrying search for IDN: {}", event.deviceIDN);
                continue;
            }
            else
            {
                Logger::logger().log_unit()->info("Found device (IDN: {}) at port {}, attempting reconnection", event.deviceIDN, port);
                node.mapped().setPort(port);
                node.mapped().connect();
                node.key() = port;
                Logger::logger().log_unit()->info("Successfully reconnected device (IDN: {}) at port {}", event.deviceIDN, port);
                devices.insert(std::move(node));
                return;
            }
        }
    }
    catch (const std::exception& e)
    {
        Logger::logger().log_unit()->error("Reconnection failed for device (IDN: {}) at port {}: {}", event.deviceIDN, event.devicePort, e.what());
        devices.insert(std::move(node));
    }
}

void UnitConfiguration::handleConfig(const std::string& message)
{
    std::lock_guard lock(channelListMutex);

    // Reject if any channel is running
    for (const auto& [channelID, channel] : channelList)
    {
        if (channel.isInProcess())
        {
            Logger::logger().log_unit()->error("Configuration rejected: channel {} has an experiment in process", channelID);
            publishFeedback("config", fmt::format("CONFIG_ERROR: channel {} has an experiment in process",channelID));
            return;
        }
    }

    // Parse
    const auto parsedConfig = parseJSONToConfig(message);

    if (!parsedConfig)
    {
        Logger::logger().log_unit()->error("Configuration rejected: invalid JSON or schema");
        publishFeedback("config", "CONFIG_ERROR: invalid JSON or schema");
        return;
    }

    const auto& newConfigMap = *parsedConfig;
    if (newConfigMap.empty())
    {
        Logger::logger().log_unit()->error("Configuration rejected: no channels were provided");
        publishFeedback("config", "CONFIG_ERROR: no channels were provided");
        return;
    }

    // Validate
    std::unordered_set<std::string> loadChannels;
    std::unordered_set<std::string> supplyPorts;
    std::unordered_set<std::string> modulePorts;

    for (const auto& [channelID, config] : newConfigMap)
    {
        if (channelID.empty() || config.loadPort.empty() || config.supplyPort.empty() || config.modulePort.empty())
        {
            Logger::logger().log_unit()->error("Configuration rejected: channel IDs and device ports must not be empty");
            publishFeedback("config", "CONFIG_ERROR: channel IDs and device ports must not be empty");
            return;
        }

        if (config.loadChannel < 1 ||
            config.loadChannel > config.totalLoadChannelCount)
        {
            Logger::logger().log_unit()->error("Invalid load channel {} for {} ({} channels)",config.loadChannel,config.loadPort,config.totalLoadChannelCount);
            publishFeedback("config", fmt::format("CONFIG_ERROR: invalid load channel {} for {}", config.loadChannel, config.loadPort));

            return;
        }

        const std::string loadChannelKey = config.loadPort + ":" + std::to_string(config.loadChannel);

        if (!loadChannels.insert(loadChannelKey).second)
        {
            Logger::logger().log_unit()->error("Duplicate load channel: {} channel {}",config.loadPort,config.loadChannel);
            publishFeedback("config", fmt::format("CONFIG_ERROR: duplicate load channel {} channel {}", config.loadPort, config.loadChannel));

            return;
        }

        if (!supplyPorts.insert(config.supplyPort).second)
        {
            Logger::logger().log_unit()->error("Duplicate supply port: {}",config.supplyPort);
            publishFeedback("config", fmt::format("CONFIG_ERROR: duplicate supply port {}", config.supplyPort));

            return;
        }

        if (!modulePorts.insert(config.modulePort).second)
        {
            Logger::logger().log_unit()->error("Duplicate module port: {}",config.modulePort);
            publishFeedback("config", fmt::format("CONFIG_ERROR: duplicate module port {}", config.modulePort));

            return;
        }
    }

    // Build new channels
    std::unordered_map<std::string, Load> newLoadList;
    std::unordered_map<std::string, Supply> newSupplyList;
    std::unordered_map<std::string, Module> newModuleList;
    std::unordered_map<std::string, Channel> newChannelList;

    try
    {
        for (const auto& [channelID, config] : newConfigMap)
        {
            newLoadList.try_emplace(
                config.loadPort,
                config.loadPort,
                config.loadBaudRate,
                config.totalLoadChannelCount,
                config.loadIDN,
                nodeID
            );

            newSupplyList.try_emplace(
                config.supplyPort,
                config.supplyPort,
                config.supplyBaudRate,
                config.supplyIDN,
                config.channelID
            );

            newModuleList.try_emplace(
                config.modulePort,
                config.modulePort,
                config.moduleBaudRate,
                config.moduleIDN,
                config.channelID
            );

            newChannelList.try_emplace(
                channelID,
                newLoadList.at(config.loadPort),
                newSupplyList.at(config.supplyPort),
                newModuleList.at(config.modulePort),
                config.loadChannel, // ChannelID is node_id+cycler_id --> 11, 12, 13.....
                channelID,
                &eventBus
            );
        }
    }
    catch (const std::exception& e)
    {
        Logger::logger().log_unit()->error("Configuration rejected while creating devices: {}", e.what());
        publishFeedback("config", fmt::format("CONFIG_ERROR: device creation failed: {}", e.what()));
        return;
    }

    // Commit
    for (auto& [id, channel] : channelList)
    {
        channel.stopWorkerThread();
    }

    loadList = std::move(newLoadList);
    supplyList = std::move(newSupplyList);
    moduleList = std::move(newModuleList);
    channelList = std::move(newChannelList);

    for (auto& [id, channel] : channelList)
    {
        channel.startWorkerThread();
    }

    Logger::logger().log_unit()->info("Configuration successfully replaced");
    publishFeedback("config", "CONFIG_OK: configuration successfully replaced");

    {
        std::lock_guard<std::mutex> lock(mutex);
        configured = true;
    }

    cv.notify_one();
}

void UnitConfiguration::handleExperiment(const std::string& message)
{
    // Parse
    const auto parsedExperiment = parseJSONToCommand(message);
    if (!parsedExperiment)
    {
        Logger::logger().log_unit()->error("Experiment rejected: invalid JSON or command schema");
        publishFeedback("experiment", "EXPERIMENT_ERROR: invalid JSON or command schema");
        return;
    }

    const auto& [channelID, commands] = *parsedExperiment;
    if (channelID.empty() || commands.empty())
    {
        Logger::logger().log_unit()->error("Experiment rejected: channel ID and at least one step are required");
        publishFeedback("experiment", "EXPERIMENT_ERROR: channel ID and at least one step are required");
        return;
    }

    // Validation
    std::lock_guard lock(channelListMutex);

    if (!channelList.contains(channelID))
    {
        Logger::logger().log_unit()->error("Experiment rejected: unknown channel {}",channelID);
        publishFeedback("experiment", fmt::format("EXPERIMENT_ERROR: unknown channel {}", channelID));
        return;
    }

    // Updation
    Channel& ch = channelList.at(channelID);
    if(ch.isInProcess())
    {
        Logger::logger().log_unit()->error("Experiment rejected: experiment already running at {}",channelID);
        publishFeedback("experiment", fmt::format("EXPERIMENT_ERROR: channel {} is already running", channelID));
        return;
    }
    
    channelList.at(channelID).updateExperiment(commands);
    Logger::logger().log_unit()->info("Experiment loaded for channel {}", channelID);
    publishFeedback("experiment", fmt::format("EXPERIMENT_OK: loaded for channel {}", channelID));
}

void UnitConfiguration::handleControl(const std::string& message)
{
    const auto parsedCommand = parseJSONToControl(message);

    if (!parsedCommand)
    {
        Logger::logger().log_unit()->error("Command rejected: invalid JSON or schema");
        publishFeedback("control", "CONTROL_ERROR: invalid JSON or schema");
        return;
    }

    const auto& commandMap = *parsedCommand;

    const auto& channelID = commandMap.at("channelID");
    const auto& command = commandMap.at("command");

    std::lock_guard lock(channelListMutex);

    if (!channelList.contains(channelID))
    {
        Logger::logger().log_unit()->error("Command rejected: unknown channel {}",channelID);
        publishFeedback("control",fmt::format("CONTROL_ERROR: unknown channel {}",channelID));
        return;
    }

    channelList.at(channelID).enqueueCommand(command);
    Logger::logger().log_unit()->info("Control command '{}' queued for channel {}", command, channelID);
    publishFeedback("control",fmt::format("CONTROL_OK: command '{}' queued for channel {}",command,channelID));
}
