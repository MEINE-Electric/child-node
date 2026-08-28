#include "drivers/load.h"
#include "drivers/serial.h"
#include "helper/logger/logger.h"

#include <fmt/format.h>

Load::Load(const std::string &port, const speed_t &baudrate, int maxChannels, const std::string &idn, const std::string alias)
: idn(idn), alias(alias), maxChannels(maxChannels), states(maxChannels), serial(port, baudrate)
{

};

Load::~Load()
{
    disconnect();
}

bool Load::connect()
{
    if(!serial.open())
    {
        Logger::logger().log_load()->error("Load-{} failed to connected at {}",alias,serial.getPort());
        return false;
    }

    if(idn!=queryIDN())
    {
        Logger::logger().log_load()->error("Load-{} IDN mismatch {}",alias,serial.getPort());
        disconnect();
        return false;
    }
        
    Logger::logger().log_load()->debug("Load-{} connected at {}",alias,serial.getPort());
    return true;
}

bool Load::disconnect()
{
    for (int i = 0; i < maxChannels; ++i)
    {
        if(states[i].outputEnabled)
        {
            disable(i+1);
        }
    }

    if(!serial.close())
    {
        Logger::logger().log_load()->error("Load-{} could not disconnect at {}",alias,serial.getPort());
        return false;
    }

    Logger::logger().log_load()->debug("Load-{} disconnected at {}",alias,serial.getPort());
    return true;
}

std::string Load::query(const std::string& command)
{
    constexpr int maxRetries = 1;

    for (int attempt = 1; attempt <= maxRetries; ++attempt)
    {
        if (!write(command))
        {
            Logger::logger().log_load()->warn("Query write attempt {}/{} failed for \"{}\" to Load-{}", attempt, maxRetries, command, alias);
            continue;
        }

        std::string response = serial.readLine();

        if (!response.empty())
        {
            Logger::logger().log_load()->debug("{} queried from Load-{} -> {}",command,alias,response);
            return response;
        }

        Logger::logger().log_load()->warn("Query read attempt {}/{} failed for \"{}\" to Load-{}", attempt, maxRetries, command, alias);
    }

    Logger::logger().log_load()->error("Failed to query command \"{}\" from Load-{} after {} attempts", command, alias, maxRetries);
    throw std::runtime_error("Failed to read response for command \"" + command + "\" from Load-" + alias);
}

bool Load::write(const std::string& command)
{
    const int maxRetries = 3;
    
    for (int attempt = 1; attempt <= maxRetries; ++attempt)
    {
        if (serial.write(command + "\n") >= 0)
        {
            Logger::logger().log_load()->debug("{} has been written to Load-{}",command,alias);
            return true;
        }

        Logger::logger().log_load()->warn("Write attempt {}/{} failed for command \"{}\" to Load-{}", attempt, maxRetries, command, alias);
    }

    Logger::logger().log_load()->error("Failed to write command \"{}\" to Load-{} after {} attempts", command, alias, maxRetries );
    throw std::runtime_error("Failed to write command \"" + command + "\" to Load-" + alias);
}

bool Load::enable(int channel)
{
    std::lock_guard<std::mutex> lock(mutex);

    if(!setChannel(channel))
    {
        return false;
    
    }

    if(!write("INP ON"))
    {
        Logger::logger().log_load()->error("Failed to enable Load-{}", alias);
        return false;
    }

    states[channel-1].outputEnabled = true;
    Logger::logger().log_load()->debug("Load-{} has been enabled", alias);
    return true;  
}

bool Load::disable(int channel)
{
    std::lock_guard<std::mutex> lock(mutex);

    if(!setChannel(channel))
    {
        return false;
    }

    if(!write("INP OFF"))
    {
        Logger::logger().log_load()->error("Failed to disable Load-{}", alias);
        return false;
    }

    states[channel-1].outputEnabled = false;
    Logger::logger().log_load()->debug("Load-{} has been disabled", alias);
    return true; 
}

bool Load::setCurrent(float current, int channel)
{
    std::lock_guard<std::mutex> lock(mutex);

    if(!setChannel(channel))
    {
        return false;
    }

    if(!write(fmt::format("CURR {}", current)))
    {
        Logger::logger().log_load()->error("Failed to set current at Load-{}", alias);
        return false;
    }

    states[channel-1].current = current;
    Logger::logger().log_load()->debug("Load-{} current has been set to {}", alias, current);
    return true; 
}

bool Load::setChannel(int channel)
{
    if(!write(fmt::format("CHAN {}", channel)))
    {
        Logger::logger().log_load()->error("Failed to set channel at Load-{}", alias);
        return false;
    }

    Logger::logger().log_load()->debug("Load-{} channel has been set to {}", alias, channel);
    return true; 
}

std::string Load::queryIDN()
{
    std::lock_guard<std::mutex> lock(mutex);
    std::string queriedIDN = query("*IDN?");

    if(queriedIDN.empty())
    {
        Logger::logger().log_load()->error("Load-{} did not return an IDN", alias);
        return "";
    }

    Logger::logger().log_load()->debug("Load-{} returned an IDN {}", alias, queriedIDN);
    return queriedIDN;
}

int Load::queryState(int channel)
{
    std::lock_guard<std::mutex> lock(mutex);

    if(!setChannel(channel))
    {
        return false;
    }

    std::string queriedState = query("INP?");

    if(queriedState.empty())
    {
        Logger::logger().log_load()->error("Load-{} did not return a valid state", alias);
        return -1;
    }

    Logger::logger().log_load()->debug("Load-{} returned {}", alias, queriedState);
    return queriedState == "ON" ? 1 : 0;
}

float Load::queryVoltage(int channel)
{
    std::lock_guard<std::mutex> lock(mutex);

    if(!setChannel(channel))
    {
        return false;
    }

    std::string queriedVoltage = query("VOLT?");

    if(queriedVoltage.empty())
    {
        Logger::logger().log_load()->error("Load-{} did not return a set voltage", alias);
        return -1;
    }

    Logger::logger().log_load()->debug("Load-{} returned a set voltage of {}V", alias, queriedVoltage);
    return std::stof(queriedVoltage);
}

float Load::queryCurrent(int channel)
{
    std::lock_guard<std::mutex> lock(mutex);

    if(!setChannel(channel))
    {
        return false;
    }

    std::string queriedCurrent = query("CURR?");

    if(queriedCurrent.empty())
    {
        Logger::logger().log_load()->error("Load-{} did not return a set current", alias);
        return -1;
    }

    Logger::logger().log_load()->debug("Load-{} returned an measured current of {}A", alias, queriedCurrent);
    return std::stof(queriedCurrent);
}

float Load::measureVoltage(int channel)
{
    std::lock_guard<std::mutex> lock(mutex);

    if(!setChannel(channel))
    {
        return false;
    }

    std::string queriedVoltage = query("MEAS:VOLT?");

    if(queriedVoltage.empty())
    {
        Logger::logger().log_load()->error("Load-{} did not return a measured voltage", alias);
        return -1;
    }

    Logger::logger().log_load()->debug("Load-{} returned a measured voltage of {}V", alias, queriedVoltage);
    return std::stof(queriedVoltage);
}

float Load::measureCurrent(int channel)
{
    std::lock_guard<std::mutex> lock(mutex);

    if(!setChannel(channel))
    {
        return false;
    }

    std::string queriedCurrent = query("MEAS:CURR?");

    if(queriedCurrent.empty())
    {
        Logger::logger().log_load()->error("Load-{} did not return a set current", alias);
        return -1;
    }

    Logger::logger().log_load()->debug("Load-{} returned a measured current of {}A", alias, queriedCurrent);
    return std::stof(queriedCurrent);
}

Load::Measurements Load::measureAll(int channel)
{
    std::lock_guard<std::mutex> lock(mutex);

    if(!setChannel(channel))
    {
        return {};
    }

    std::string queriedMeasurements = query("MEAS:REAL?");
    Measurements results = {};

    if(queriedMeasurements.empty())
    {
        Logger::logger().log_load()->error("Load-{} did not return any measurements", alias);
        return {};
    }

    std::stringstream ss(queriedMeasurements);
    std::string token;

    try
    {
        std::getline(ss, token, ',');
        results.voltage = std::stof(token);
        std::getline(ss, token, ',');
        results.current = std::stof(token);
        std::getline(ss, token, ',');
        results.power = std::stof(token);
    }
    catch (const std::exception& e)
    {
        Logger::logger().log_load()->error("Load-{} returned invalid measurement string: {}", alias, queriedMeasurements);
        return {};
    }
    
    Logger::logger().log_load()->debug("Load-{} returned measurements {}", alias, queriedMeasurements);
    return results;
}

bool Load::restoreState()
{
    for (int i = 1; i <= maxChannels; ++i){
        State queriedState
        {
            queryState(i) == 1,
            queryCurrent(i)
        };
        
        bool changed = false;

        if (queriedState.current < 0.0f)
        {
            Logger::logger().log_load()->error("Load-{} failed to query current state",alias);
            return false;
        }

        constexpr float epsilon = 0.001f;

        if (std::fabs(queriedState.current - states[i-1].current) > epsilon)
        {
            if (!setCurrent(states[i-1].current, i))
                return false;

            changed = true;
        }

        if (queriedState.outputEnabled != states[i-1].outputEnabled)
        {
            if (states[i-1].outputEnabled)
            {
                if (!enable(i)) // Load channels expect from 1....maxCHannels
                    return false;
            }
            else
            {
                if (!disable(i)) 
                    return false;
            }
            
            changed = true;
        }

        if (changed){
            Logger::logger().log_load()->debug("Load-{} state restored",alias);
        }
        else 
        {
            Logger::logger().log_load()->debug("Load-{} state already correct",alias);
        }
    }

    return true;
}
