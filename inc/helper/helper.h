#pragma once

#include <unordered_map>
#include <variant>
#include <vector>
#include <string>
#include <optional>
#include <nlohmann/json.hpp>

// -------- Command struct guide --------
struct Charge
{
    float voltage;
    float current;
    float cutoffVoltage;
    int duration;      
};

struct Discharge
{
    float current;
    float cutoffVoltage;
    int duration;      
};

struct Hold
{
    float cutoffVoltage;
    int duration;      
};

struct Rest
{
    int duration;     
};

struct Goto
{
    int targetStep;
    int repeatCount;
};

using Command = std::variant<Charge,Discharge,Hold,Rest,Goto>;

// -------- Channel config helper --------
struct Config
{
    std::string channelID;

    // Instruments
    std::string loadPort;
    std::string loadIDN;
    int totalLoadChannelCount;
    int loadChannel;
    int loadBaudRate;

    std::string supplyPort;
    std::string supplyIDN;
    int supplyBaudRate;

    std::string modulePort;
    std::string moduleIDN;
    int moduleBaudRate;
};

// -------- String to Command parser --------
std::optional<std::pair<std::string, std::vector<Command>>> parseJSONToCommand(const std::string& json);

// -------- String to Command parser --------
std::optional<std::unordered_map<std::string, Config>> parseJSONToConfig(const std::string& json);

// -------- String to Control Command parser --------
std::optional<std::unordered_map<std::string, std::string>> parseJSONToControl(const std::string& json);
