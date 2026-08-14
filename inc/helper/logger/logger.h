#pragma once

#include <memory>
#include <spdlog/logger.h>

class Logger
{
public:
    static Logger& logger();

    void init();

    std::shared_ptr<spdlog::logger> log_channel();
    std::shared_ptr<spdlog::logger> log_unit();
    std::shared_ptr<spdlog::logger> log_json();
    std::shared_ptr<spdlog::logger> log_mqtt();
    std::shared_ptr<spdlog::logger> log_serial();
    std::shared_ptr<spdlog::logger> log_registry();
    std::shared_ptr<spdlog::logger> log_supply();
    std::shared_ptr<spdlog::logger> log_load();
    std::shared_ptr<spdlog::logger> log_module();

private:
    Logger();
    ~Logger() = default;

    Logger(const Logger&) = delete;
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
