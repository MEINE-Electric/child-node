#include "helper/logger/logger.h"

#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/rotating_file_sink.h>

// Helper Functions
std::shared_ptr<spdlog::logger> register_logger(const std::string& name, const std::vector<spdlog::sink_ptr>& sinks){
    return std::make_shared<spdlog::logger>(name,sinks.begin(),sinks.end());
}   

Logger& Logger::logger()
{
    static Logger instance;
    return instance;
}

Logger::Logger()
{
}

void Logger::init()
{
    auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    auto file_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>("logs/app.log", 5 * 1024 * 1024, 5, false);

    console_sink->set_level(spdlog::level::debug);
    file_sink->set_level(spdlog::level::debug);

    std::vector<spdlog::sink_ptr> sinks {
        console_sink,
        file_sink
    };

    channel = register_logger("CHANNEL ", sinks);
    unit = register_logger("UNIT    ", sinks);
    json = register_logger("JSON    ", sinks);
    mqtt = register_logger("MQTT    ", sinks);
    serial = register_logger("SERIAL  ", sinks);
    registry = register_logger("REGISTRY", sinks);
    supply = register_logger("SUPPLY  ", sinks);
    load = register_logger("LOAD    ", sinks);
    module = register_logger("MODULE  ", sinks);

    channel->set_level(spdlog::level::debug);
    unit->set_level(spdlog::level::debug);
    json->set_level(spdlog::level::debug);
    mqtt->set_level(spdlog::level::info);
    serial->set_level(spdlog::level::info);
    registry->set_level(spdlog::level::info);
    supply->set_level(spdlog::level::info);
    load->set_level(spdlog::level::info);
    module->set_level(spdlog::level::info);

    channel->set_pattern("%T | %^%n%$ [%^%l%$] %v");
    unit->set_pattern("%T | %^%n%$ [%^%l%$] %v");
    json->set_pattern("%T | %^%n%$ [%^%l%$] %v");
    mqtt->set_pattern("%T | %^%n%$ [%^%l%$] %v");
    serial->set_pattern("%T | %^%n%$ [%^%l%$] %v");
    registry->set_pattern("%T | %^%n%$ [%^%l%$] %v");
    supply->set_pattern("%T | %^%n%$ [%^%l%$] %v");
    load->set_pattern("%T | %^%n%$ [%^%l%$] %v");
    module->set_pattern("%T | %^%n%$ [%^%l%$] %v");
}

std::shared_ptr<spdlog::logger> Logger::log_channel()
{
    return channel;
}

std::shared_ptr<spdlog::logger> Logger::log_unit()
{
    return unit;
}

std::shared_ptr<spdlog::logger> Logger::log_json()
{
    return json;
}

std::shared_ptr<spdlog::logger> Logger::log_mqtt()
{
    return mqtt;
}

std::shared_ptr<spdlog::logger> Logger::log_serial()
{
    return serial;
}

std::shared_ptr<spdlog::logger> Logger::log_registry()
{
    return registry;
}

std::shared_ptr<spdlog::logger> Logger::log_supply()
{
    return supply;
}

std::shared_ptr<spdlog::logger> Logger::log_load()
{
    return load;
}

std::shared_ptr<spdlog::logger> Logger::log_module()
{
    return module;
}
