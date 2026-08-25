#pragma once

#include <memory>
#include <spdlog/logger.h>

/**
 * @brief Centralized logging manager for the application.
 *
 * Provides access to separate spdlog loggers for different application
 * subsystems, including channels, units, JSON parsing, MQTT, serial
 * communication, instrument drivers, and the registry.
 *
 * Logger follows the Singleton pattern. The singleton instance is
 * accessed through logger().
 */
class Logger
{
public:

    /**
     * @brief Returns the global Logger instance.
     *
     * Creates the Logger instance on first use and returns the same
     * instance for all subsequent calls.
     *
     * @return Reference to the global Logger instance.
     */
    static Logger& logger();

    /**
     * @brief Initializes all application loggers.
     *
     * Creates console and rotating file sinks and configures the
     * subsystem-specific loggers with their logging levels and output
     * patterns.
     *
     * This function should be called once during application startup
     * before using any of the logger accessors.
     */
    void init();

    /**
     * @brief Returns the channel logger.
     *
     * @return Shared pointer to the channel logger.
     */
    std::shared_ptr<spdlog::logger> log_channel();

    /**
     * @brief Returns the unit logger.
     *
     * @return Shared pointer to the unit logger.
     */
    std::shared_ptr<spdlog::logger> log_unit();

    /**
     * @brief Returns the JSON parser logger.
     *
     * @return Shared pointer to the JSON logger.
     */
    std::shared_ptr<spdlog::logger> log_json();

    /**
     * @brief Returns the MQTT logger.
     *
     * @return Shared pointer to the MQTT logger.
     */
    std::shared_ptr<spdlog::logger> log_mqtt();

    /**
     * @brief Returns the serial communication logger.
     *
     * @return Shared pointer to the serial logger.
     */
    std::shared_ptr<spdlog::logger> log_serial();

    /**
     * @brief Returns the registry logger.
     *
     * @return Shared pointer to the registry logger.
     */
    std::shared_ptr<spdlog::logger> log_registry();

    /**
     * @brief Returns the power supply logger.
     *
     * @return Shared pointer to the supply logger.
     */
    std::shared_ptr<spdlog::logger> log_supply();

    /**
     * @brief Returns the electronic load logger.
     *
     * @return Shared pointer to the load logger.
     */
    std::shared_ptr<spdlog::logger> log_load();

    /**
     * @brief Returns the control module logger.
     *
     * @return Shared pointer to the module logger.
     */
    std::shared_ptr<spdlog::logger> log_module();

private:

    /**
     * @brief Constructs the Logger manager.
     *
     * Private because Logger is implemented as a Singleton and should
     * only be instantiated through logger().
     */
    Logger();

    /**
     * @brief Destroys the Logger manager.
     */
    ~Logger() = default;

    /**
     * @brief Prevents copying of the Logger singleton.
     */
    Logger(const Logger&) = delete;

    /**
     * @brief Prevents assignment of the Logger singleton.
     */
    Logger& operator=(const Logger&) = delete;

    std::shared_ptr<spdlog::logger> channel;
    std::shared_ptr<spdlog::logger> unit;
    std::shared_ptr<spdlog::logger> json;
    std::shared_ptr<spdlog::logger> mqtt;
    std::shared_ptr<spdlog::logger> serial;
    std::shared_ptr<spdlog::logger> registry;
    std::shared_ptr<spdlog::logger> supply;
    std::shared_ptr<spdlog::logger> load;
    std::shared_ptr<spdlog::logger> module;
};