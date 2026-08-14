#include <unordered_map>
#include <vector>
#include <string>
#include <nlohmann/json.hpp>
#include <optional>

#include "helper/helper.h"
#include "helper/logger/logger.h"

// String to Command parser
std::optional<std::pair<std::string, std::vector<Command>>> parseJSONToCommand(const std::string& json)
{
    std::vector<Command> commands;
    std::string channelID;

    try
    {
        const nlohmann::json parsedJson = nlohmann::json::parse(json);

        channelID = parsedJson.at("channelID").get<std::string>();

        for (const auto& i : parsedJson.at("steps"))
        {
            const std::string command =
                i.at("command").get<std::string>();

            if (command == "Charge")
            {
                commands.emplace_back(
                    Charge{
                        i.at("voltage").get<float>(),
                        i.at("cutoff").get<float>(),
                        i.at("cutoffVoltage").get<float>(),
                        i.at("duration").get<int>()
                    }
                );
            }
            else if (command == "Discharge")
            {
                commands.emplace_back(
                    Discharge{
                        i.at("cutoff").get<float>(),
                        i.at("cutoffVoltage").get<float>(),
                        i.at("duration").get<int>()
                    }
                );
            }
            else if (command == "Hold")
            {
                commands.emplace_back(
                    Hold{
                        i.at("voltage").get<float>(),
                        i.at("duration").get<int>()
                    }
                );
            }
            else if (command == "Rest")
            {
                commands.emplace_back(
                    Rest{
                        i.at("duration").get<int>()
                    }
                );
            }
            else if (command == "Goto")
            {
                commands.emplace_back(
                    Goto{
                        i.at("targetSteps").get<int>(),
                        i.at("additionalRepeats").get<int>()
                    }
                );
            }
            else
            {
                Logger::logger().log_json()->error(
                    "Unknown command: {}", command
                );

                return std::nullopt;
            }
        }
    }
    catch (const nlohmann::json::exception& e)
    {
        Logger::logger().log_json()->error(
            "JSON error: {}", e.what()
        );
        return std::nullopt;
    }

    return std::make_pair(channelID, std::move(commands));
}

// String to Config parser
std::optional<std::unordered_map<std::string, Config>> parseJSONToConfig(const std::string& message)
{
    try
    {
        const nlohmann::json jsonParsed = nlohmann::json::parse(message);

        if (!jsonParsed.is_array())
        {
            Logger::logger().log_json()->error("Config must be a JSON array");
            return std::nullopt;
        }

        std::unordered_map<std::string, Config> newConfig;

        for (const auto& config : jsonParsed)
        {
            Config tempConfig;

            tempConfig.channelID = config.at("channelID").get<std::string>();

            tempConfig.loadPort = config.at("loadPort").get<std::string>();
            tempConfig.loadBaudRate = config.at("loadBaudRate").get<int>();
            tempConfig.loadIDN = config.at("loadIDN").get<std::string>();
            tempConfig.totalLoadChannelCount = config.at("totalLoadChannelCount").get<int>();
            tempConfig.loadChannel = config.at("loadChannel").get<int>();

            tempConfig.supplyPort = config.at("supplyPort").get<std::string>();
            tempConfig.supplyBaudRate = config.at("supplyBaudRate").get<int>();
            tempConfig.supplyIDN = config.at("supplyIDN").get<std::string>();

            tempConfig.modulePort = config.at("modulePort").get<std::string>();
            tempConfig.moduleBaudRate = config.at("moduleBaudRate").get<int>();
            tempConfig.moduleIDN = config.at("moduleIDN").get<std::string>();

            if (!newConfig.emplace(tempConfig.channelID, std::move(tempConfig)).second)
            {
                Logger::logger().log_json()->error("Duplicate channel ID in configuration");
                return std::nullopt;
            }
        }

        return newConfig;
    }
    catch (const nlohmann::json::exception& e)
    {
        Logger::logger().log_json()->error(
            "Failed to parse config: {}",
            e.what()
        );

        return std::nullopt;
    }
}
