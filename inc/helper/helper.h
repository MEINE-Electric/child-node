#pragma once

#include <unordered_map>
#include <variant>
#include <vector>
#include <string>
#include <optional>

#include <nlohmann/json.hpp>

/**
 * @brief Represents a battery charging operation.
 *
 * Sets the supply voltage and load current while monitoring the
 * battery voltage until the configured cutoff voltage is reached
 * or the specified duration expires.
 */
struct Charge
{
    float voltage;
    float current;
    float cutoffVoltage;
    int duration;
};

/**
 * @brief Represents a battery discharge operation.
 *
 * Applies the configured discharge current while monitoring the
 * battery voltage until the cutoff voltage is reached or the
 * specified duration expires.
 */
struct Discharge
{
    float current;
    float cutoffVoltage;
    int duration;
};

/**
 * @brief Represents a battery voltage hold operation.
 *
 * Maintains the configured voltage for the specified duration.
 */
struct Hold
{
    float cutoffVoltage;
    int duration;
};

/**
 * @brief Represents a rest operation.
 *
 * Leaves the battery channel idle for the specified duration.
 */
struct Rest
{
    int duration;
};

/**
 * @brief Represents a jump to another experiment step.
 *
 * Allows an experiment to repeat a previous section by jumping to
 * the specified target step and applying the requested number of
 * additional repetitions.
 */
struct Goto
{
    int targetStep;
    int repeatCount;
};

/**
 * @brief Represents any supported experiment command.
 *
 * A Command can contain one of Charge, Discharge, Hold, Rest, or Goto.
 */
using Command = std::variant<
    Charge,
    Discharge,
    Hold,
    Rest,
    Goto
>;

/**
 * @brief Stores the hardware configuration for a battery channel.
 *
 * Contains the serial communication settings and identification
 * information for the load, power supply, and control module associated
 * with a channel.
 */
struct Config
{
    /** @brief Identifier assigned to the battery channel. */
    std::string channelID;

    /** @brief Serial port used by the electronic load. */
    std::string loadPort;

    /** @brief Expected identification string of the electronic load. */
    std::string loadIDN;

    /** @brief Total number of channels supported by the electronic load. */
    int totalLoadChannelCount;

    /** @brief Load channel assigned to this battery channel. */
    int loadChannel;

    /** @brief Baud rate used to communicate with the electronic load. */
    int loadBaudRate;

    /** @brief Serial port used by the power supply. */
    std::string supplyPort;

    /** @brief Expected identification string of the power supply. */
    std::string supplyIDN;

    /** @brief Baud rate used to communicate with the power supply. */
    int supplyBaudRate;

    /** @brief Serial port used by the control module. */
    std::string modulePort;

    /** @brief Expected identification string of the control module. */
    std::string moduleIDN;

    /** @brief Baud rate used to communicate with the control module. */
    int moduleBaudRate;
};

/**
 * @brief Parses a JSON experiment description into executable commands.
 *
 * Parses the JSON representation of an experiment, extracts its channel
 * identifier, and converts each experiment step into the corresponding
 * Command variant.
 *
 * Supported commands are Charge, Discharge, Hold, Rest, and Goto.
 *
 * @param json JSON string containing the channel ID and experiment steps.
 *
 * @return A pair containing the channel ID and parsed command list.
 *         Returns std::nullopt if the JSON is invalid, required fields
 *         are missing, or an unknown command is encountered.
 */
std::optional<std::pair<std::string, std::vector<Command>>>
parseJSONToCommand(const std::string& json);

/**
 * @brief Parses a JSON hardware configuration.
 *
 * Converts a JSON array of channel configurations into a map indexed
 * by channel ID.
 *
 * Duplicate channel IDs are rejected.
 *
 * @param json JSON string containing the channel configuration array.
 *
 * @return Map of channel IDs to Config objects, or std::nullopt if the
 *         JSON is invalid, required fields are missing, or duplicate
 *         channel IDs are found.
 */
std::optional<std::unordered_map<std::string, Config>>
parseJSONToConfig(const std::string& json);

/**
 * @brief Parses a JSON control command.
 *
 * Extracts the channel identifier and control command from a JSON
 * message.
 *
 * @param json JSON string containing a channelID and command.
 *
 * @return Map containing the "channelID" and "command" values, or
 *         std::nullopt if the JSON is invalid or required fields
 *         are missing.
 */
std::optional<std::unordered_map<std::string, std::string>>
parseJSONToControl(const std::string& json);