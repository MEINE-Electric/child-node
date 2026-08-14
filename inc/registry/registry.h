#pragma once

#include <cstddef>
#include <stop_token>
#include <string>
#include <termios.h>
#include <unordered_map>
#include <thread>
#include <mutex>

class Registry
{
public:
    // -------- Helper Structs --------
    struct Instrument
    {
        std::string idn;
        std::string port;
        speed_t baudrate;
        bool available;
    };

    struct Identification
    {
        std::string idn;
        speed_t baudrate;
    };

    // -------- Constructor/Deconstructor --------
    Registry() = default;
    ~Registry() = default;

    // -------- Scan Functions --------
    bool initialScan();
    Identification getIdentification(const std::string& port);

    // -------- Thread Functions --------
    void startWatchdog();
    void watchdog(std::stop_token stop);
    
    // -------- Read Function --------
    std::unordered_map<std::string, Instrument> readDevices();
    size_t readDeviceCount();

    // -------- Set Functions --------
    void setToAvailable(std::string& port);
    void setToUnavailable(std::string& port);

private:
    std::unordered_map<std::string, Instrument> devices;

    std::jthread watchdogThread;
    std::mutex mutex;
};