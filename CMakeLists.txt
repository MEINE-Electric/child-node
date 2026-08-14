cmake_minimum_required(VERSION 3.16)

project(app LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(spdlog REQUIRED)
find_package(PahoMqttCpp REQUIRED)
find_package(nlohmann_json REQUIRED)

set(SOURCES
    src/main.cpp
    src/helper/logger/logger.cpp
    src/mqtt/mqtt.cpp
    src/drivers/serial.cpp
    src/drivers/supply.cpp
    src/drivers/load.cpp
    src/drivers/module.cpp
    src/registry/registry.cpp
    src/unit/channel/channel.cpp
    src/unit/configuration/configuration.cpp
    src/helper/helper.cpp
)

add_executable(${PROJECT_NAME}
    ${SOURCES}
)

target_include_directories(${PROJECT_NAME}
    PRIVATE
        ${CMAKE_SOURCE_DIR}/inc
)

target_link_libraries(${PROJECT_NAME}
    PRIVATE
        spdlog::spdlog
        PahoMqttCpp::paho-mqttpp3
        nlohmann_json::nlohmann_json
)
