#include "drivers/supply.h"
#include "drivers/serial.h"
#include "helper/logger/logger.h"

#include <fmt/format.h>
#include <sstream>
#include <cmath>

Supply::Supply(const std::string &port, const speed_t &baudrate, const std::string &idn, const std::string alias)
:idn(idn), alias(alias), serial(port, baudrate){}

Supply::~Supply()
{
    disconnect();
}

bool Supply::connect()
{
    if(!serial.open())
    {
        Logger::logger().log_supply()->error("Supply-{} failed to connected at {}",alias,serial.getPort());
        return false;
    }

    if(idn!=queryIDN())
    {
        Logger::logger().log_supply()->error("Supply-{} IDN mismatch {}",alias,serial.getPort());
        disconnect();
        return false;
    }
        
    Logger::logger().log_supply()->debug("Supply-{} connected at {}",alias,serial.getPort());
    return true;
}

bool Supply::disconnect()
{
    if(state.outputEnabled)
    {
        disable();
    }

    if(!serial.close())
    {
        Logger::logger().log_supply()->error("Supply-{} could not disconnect at {}",alias,serial.getPort());
        return false;
    }

    Logger::logger().log_supply()->debug("Supply-{} disconnected at {}",alias,serial.getPort());
    return true;
}

std::string Supply::query(const std::string& command)
{
    constexpr int maxRetries = 1;

    for (int attempt = 1; attempt <= maxRetries; ++attempt)
    {
        if (!write(command))
        {
            Logger::logger().log_supply()->warn("Query write attempt {}/{} failed for \"{}\" to Supply-{}", attempt, maxRetries, command, alias);
            continue;
        }

        std::string response = serial.readLine();

        if (!response.empty())
        {
            Logger::logger().log_supply()->debug("{} queried from Supply-{} -> {}",command,alias,response);
            return response;
        }

        Logger::logger().log_supply()->warn("Query read attempt {}/{} failed for \"{}\" to Supply-{}", attempt, maxRetries, command, alias);
    }

    Logger::logger().log_supply()->error("Failed to query command \"{}\" from Supply-{} after {} attempts", command, alias, maxRetries);

    return "";
}

bool Supply::write(const std::string& command)
{
    const int maxRetries = 3;

    for (int attempt = 1; attempt <= maxRetries; ++attempt)
    {
        if (serial.write(command + "\n") >= 0)
        {
            Logger::logger().log_supply()->debug("{} has been written to Supply-{}",command,alias);
            return true;
        }

        Logger::logger().log_supply()->warn("Write attempt {}/{} failed for command \"{}\" to Supply-{}", attempt, maxRetries, command, alias);
    }

    Logger::logger().log_supply()->error("Failed to write command \"{}\" to Supply-{} after {} attempts", command, alias, maxRetries );

    return false;
}

bool Supply::enable()
{
    if(!write("OUTP ON"))
    {
        Logger::logger().log_supply()->error("Failed to enable Supply-{}", alias);
        return false;
    }

    state.outputEnabled = true;
    Logger::logger().log_supply()->debug("Supply-{} has been enabled", alias);
    return true;
}

bool Supply::disable()
{
    if(!write("OUTP OFF"))
    {
        Logger::logger().log_supply()->error("Failed to disable Supply-{}", alias);
        return false;
    }

    state.outputEnabled = false;
    Logger::logger().log_supply()->debug("Supply-{} has been disabled", alias);
    return true;
}

bool Supply::setVoltage(float voltage)
{
    if(!write(fmt::format("VOLT {}", voltage)))
    {
        Logger::logger().log_supply()->error("Supply-{} voltage cannot be set", alias);
        return false;  
    }

    state.voltage = voltage;
    Logger::logger().log_supply()->debug("Supply-{} voltage has been set to {}", alias, voltage);
    return true;
}

bool Supply::setCurrent(float current)
{
    if(!write(fmt::format("CURR {}", current)))
    {
        Logger::logger().log_supply()->error("Supply-{} current cannot be set", alias);
        return false;  
    }

    state.current = current;
    Logger::logger().log_supply()->debug("Supply-{} current has been set to {}", alias, current);
    return true;
}

std::string Supply::queryIDN()
{
    std::string queriedIDN = query("*IDN?");

    if(queriedIDN.empty())
    {
        Logger::logger().log_supply()->error("Supply-{} did not return an IDN", alias);
        return "";
    }

    Logger::logger().log_supply()->debug("Supply-{} returned an IDN {}", alias, queriedIDN);
    return queriedIDN;
}

int Supply::queryState()
{
    std::string queriedState = query("OUTP?");

    if(queriedState.empty())
    {
        Logger::logger().log_supply()->error("Supply-{} did not return a valid state", alias);
        return -1;
    }

    Logger::logger().log_supply()->debug("Supply-{} returned {}", alias, queriedState);
    return queriedState == "ON" ? 1 : 0;
}

float Supply::queryVoltage()
{
    std::string queriedVoltage = query("VOLT?");

    if(queriedVoltage.empty())
    {
        Logger::logger().log_supply()->error("Supply-{} did not return a set voltage", alias);
        return -1;
    }

    Logger::logger().log_supply()->debug("Supply-{} returned a set voltage of {}V", alias, queriedVoltage);
    return std::stof(queriedVoltage);
}  

float Supply::queryCurrent()
{
    std::string queriedCurrent = query("CURR?");

    if(queriedCurrent.empty())
    {
        Logger::logger().log_supply()->error("Supply-{} did not return a set current", alias);
        return -1;
    }

    Logger::logger().log_supply()->debug("Supply-{} returned an measured current of {}A", alias, queriedCurrent);
    return std::stof(queriedCurrent);
}

float Supply::measureVoltage()
{
    std::string queriedVoltage = query("MEAS:VOLT?");

    if(queriedVoltage.empty())
    {
        Logger::logger().log_supply()->error("Supply-{} did not return a measured voltage", alias);
        return -1;
    }

    Logger::logger().log_supply()->debug("Supply-{} returned a measured voltage of {}V", alias, queriedVoltage);
    return std::stof(queriedVoltage);
}  

float Supply::measureCurrent()
{
    std::string queriedCurrent = query("MEAS:CURR?");

    if(queriedCurrent.empty())
    {
        Logger::logger().log_supply()->error("Supply-{} did not return a set current", alias);
        return -1;
    }

    Logger::logger().log_supply()->debug("Supply-{} returned a measured current of {}A", alias, queriedCurrent);
    return std::stof(queriedCurrent);
}

Supply::Measurements Supply::measureAll()
{
    std::string queriedMeasurements = query("MEAS:ALL?");
    Measurements results = {};

    if(queriedMeasurements.empty())
    {
        Logger::logger().log_supply()->error("Supply-{} did not return any measurements", alias);
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
        Logger::logger().log_supply()->error("Supply-{} returned invalid measurement string: {}", alias, queriedMeasurements);
        return {};
    }
    
    Logger::logger().log_supply()->debug("Supply-{} returned measurements {}", alias, queriedMeasurements);
    return results;
}

bool Supply::restoreState()
{
    State queriedState
    {
        queryState() == 1,
        queryVoltage(),
        queryCurrent()
    };
    
    bool changed = false;

    if (queriedState.voltage < 0.0f || queriedState.current < 0.0f)
    {
        Logger::logger().log_supply()->error("Supply-{} failed to query current state", alias);
        return false;
    }

    constexpr float epsilon = 0.001f;

    if (std::fabs(queriedState.voltage - state.voltage) > epsilon)
    {
        if (!setVoltage(state.voltage))
            return false;

        changed = true;
    }

    if (std::fabs(queriedState.current - state.current) > epsilon)
    {
        if (!setCurrent(state.current))
            return false;

        changed = true;
    }

    if (queriedState.outputEnabled != state.outputEnabled)
    {
        if (state.outputEnabled)
        {
            if (!enable())
                return false;
        }
        else
        {
            if (!disable())
                return false;
        }
        
        changed = true;
    }

    if (changed){
        Logger::logger().log_supply()->debug("Supply-{} state restored",alias);
    }
    else 
    {
        Logger::logger().log_supply()->debug("Supply-{} state already correct",alias);
    }

    return true;
}
