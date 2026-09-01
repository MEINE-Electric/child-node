#include "drivers/module.h"
#include "drivers/serial.h"
#include "helper/logger/logger.h"

#include <fmt/format.h>
#include <thread>
#include <chrono>

Module::Module(const std::string &port, const speed_t &baudrate, const std::string &idn, const std::string alias)
:idn(idn), alias(alias), serial(port, baudrate), state(OFF){}

Module::~Module()
{
    disconnect();
};

bool Module::connect()
{
    if (!serial.open())
    {
        Logger::logger().log_module()->error(
            "Module-{} failed to connect at {}", alias, serial.getPort());
        return false;
    }

    // Give the Arduino time to reboot.
    std::this_thread::sleep_for(std::chrono::seconds(2));

    if (!idn.empty() && idn != queryIDN())
    {
        Logger::logger().log_module()->error(
            "Module-{} IDN mismatch {}", alias, serial.getPort());

        disconnect();
        return false;
    }

    Logger::logger().log_module()->debug(
        "Module-{} connected at {}", alias, serial.getPort());

    return true;
}

bool Module::disconnect()
{
    if(!serial.close())
    {
        Logger::logger().log_module()->error("Module-{} could not disconnect at {}",alias,serial.getPort());
        return false;
    }

    Logger::logger().log_module()->debug("Module-{} disconnected at {}",alias,serial.getPort());
    return true;
}

bool Module::isConnected()
{
    return serial.isOpen();
}

std::string Module::query(const std::string& command)
{
    constexpr int maxRetries = 1;

    for (int attempt = 1; attempt <= maxRetries; ++attempt)
    {
        if (!write(command))
        {
            Logger::logger().log_module()->warn("Query write attempt {}/{} failed for \"{}\" to Module-{}", attempt, maxRetries, command, alias);
            continue;
        }

        std::string response = serial.readLine();

        if (!response.empty())
        {
            Logger::logger().log_module()->debug("{} queried from Module-{} -> {}",command,alias,response);
            return response;
        }

        Logger::logger().log_module()->warn("Query read attempt {}/{} failed for \"{}\" to Module-{}", attempt, maxRetries, command, alias);
    }

    Logger::logger().log_module()->error("Failed to query command \"{}\" from Module-{} after {} attempts", command, alias, maxRetries);

    throw std::runtime_error("Failed to read response for command \"" + command + "\" from Module-" + alias);
}

bool Module::write(const std::string& command)
{
    const int maxRetries = 3;

    for (int attempt = 1; attempt <= maxRetries; ++attempt)
    {
        if (serial.write(command + "\n") >= 0)
        {
            Logger::logger().log_module()->debug("{} has been written to Module-{}",command,alias);
            return true;
        }

        Logger::logger().log_module()->warn("Write attempt {}/{} failed for command \"{}\" to Module-{}", attempt, maxRetries, command, alias);
    }

    Logger::logger().log_module()->error("Failed to write command \"{}\" to Module-{} after {} attempts", command, alias, maxRetries );

    throw std::runtime_error("Failed to write command \"" + command + "\" to Module-" + alias);
}

bool Module::setToCharge()
{
    if(!write("STATE CHARGE"))
    {
        Logger::logger().log_module()->error("Module-{} cannot be set to charging", alias);
        return false;  
    }

    state = CHARGE;
    Logger::logger().log_module()->debug("Module-{} current has been set to charging", alias);
    return true;
}

bool Module::setToDischarge()
{
    if(!write("STATE DISCHARGE"))
    {
        Logger::logger().log_module()->error("Module-{} cannot be set to discharging", alias);
        return false;  
    }

    state = DISCHARGE;
    Logger::logger().log_module()->debug("Module-{} current has been set to discharging", alias);
    return true;
}

bool Module::setToOff()
{
    if(!write("STATE OFF"))
    {
        Logger::logger().log_module()->error("Module-{} cannot be set to off", alias);
        return false;  
    }

    state = OFF;
    Logger::logger().log_module()->debug("Module-{} current has been set to off", alias);
    return true;
}

std::string Module::queryIDN()
{
    std::string queriedIDN = query("*IDN?");

    if(queriedIDN.empty())
    {
        Logger::logger().log_module()->error("Module-{} did not return an IDN", alias);
        return "";
    }

    Logger::logger().log_module()->debug("Module-{} returned an IDN {}", alias, queriedIDN);
    return queriedIDN;
}

Module::State Module::queryState() // Sends back the state stored by the hardware
{
    std::string queriedState = query("STATE?");

    if(queriedState.empty())
    {
        Logger::logger().log_module()->error("Module-{} did not return a state", alias);
        return State::ERROR;
    }

    Logger::logger().log_module()->debug("Module-{} returned a state {}", alias, queriedState);

    if(queriedState == "Off")
        return State::OFF;
    else if(queriedState == "Charge")
        return State::CHARGE;
    else if(queriedState == "Discharge")
        return State::DISCHARGE;
    else
        return State::ERROR;
}
