#include <string>
#include <exception>
#include <unordered_set>

#include "mqtt/mqtt.h"
#include "helper/helper.h"
#include "helper/logger/logger.h"
#include "unit/configuration/configuration.h"

UnitConfiguration::UnitConfiguration(std::string nodeID, MQTT& mqtt)
: nodeID(nodeID), mqtt(mqtt)
{
}

UnitConfiguration::~UnitConfiguration()
{
    stopEventWatchdog();
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
        while (!stop.stop_requested())
        {
            checkForEvents();
        }
    });

    Logger::logger().log_unit()->info("Event watchdog started");
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

void UnitConfiguration::publishFeedback(const std::string& section, const std::string& message)
{
    mqtt.publish(fmt::format("{}/{}/feedback", section, nodeID), message);
}

void UnitConfiguration::checkForEvents()
{
    const auto newEvent = mqtt.waitForEventFor(std::chrono::milliseconds(100));
    if (!newEvent)
    {
        return;
    }

    if(newEvent->first == "config")
    {
        handleConfig(newEvent->second);
    }
    else if(newEvent->first == "experiment")
    {
        handleExperiment(newEvent->second);
    }
}

void UnitConfiguration::handleConfig(const std::string& message)
{
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
                config.loadChannel,
                channelID
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
    loadList = std::move(newLoadList);
    supplyList = std::move(newSupplyList);
    moduleList = std::move(newModuleList);
    channelList = std::move(newChannelList);

    Logger::logger().log_unit()->info("Configuration successfully replaced");
    publishFeedback("config", "CONFIG_OK: configuration successfully replaced");
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
    if (!channelList.contains(channelID))
    {
        Logger::logger().log_unit()->error("Experiment rejected: unknown channel {}",channelID);
        publishFeedback("experiment", fmt::format("EXPERIMENT_ERROR: unknown channel {}", channelID));
        return;
    }

    // Updation
    Channel& ch = channelList.at(channelID);
    if(ch.isRunning())
    {
        Logger::logger().log_unit()->error("Experiment rejected: experiment already running at {}",channelID);
        publishFeedback("experiment", fmt::format("EXPERIMENT_ERROR: channel {} is already running", channelID));
        return;
    }
    
    channelList.at(channelID).updateExperiment(commands);
    Logger::logger().log_unit()->info("Experiment loaded for channel {}", channelID);
    publishFeedback("experiment", fmt::format("EXPERIMENT_OK: loaded for channel {}", channelID));
}
