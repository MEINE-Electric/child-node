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

    /**
     * @brief Represents a serial-connected instrument discovered by the registry.
     *
     * Stores the instrument identification, serial port, communication speed,
     * and whether the instrument is currently available for use.
     */
    struct Instrument
    {
        std::string idn;
        std::string port;
        speed_t baudrate;
        bool available;
    };

    /**
     * @brief Identification information returned by an instrument.
     *
     * Contains the instrument's SCPI identification string and the baud rate
     * at which the instrument responded.
     */
    struct Identification
    {
        std::string idn;
        speed_t baudrate;
    };

    // -------- Constructor/Destructor --------

    /**
     * @brief Constructs an empty instrument registry.
     */
    Registry() = default;

    /**
     * @brief Destroys the registry.
     *
     * The watchdog thread is automatically stopped and joined by std::jthread.
     */
    ~Registry() = default;

    // -------- Scan Functions --------

    /**
     * @brief Performs the initial scan for connected serial instruments.
     *
     * Scans /dev for ttyUSB devices and attempts to identify each device
     * using the SCPI *IDN? command at supported baud rates.
     *
     * Successfully identified devices are added to the registry.
     *
     * @return true if at least one serial device was discovered;
     *         false if no devices were found.
     */
    bool initialScan();

    /**
     * @brief Identifies an instrument connected to a serial port.
     *
     * Attempts to communicate with the specified port using the supported
     * baud rates and sends the SCPI *IDN? command.
     *
     * @param port Serial device path, for example "/dev/ttyUSB0".
     *
     * @return Identification containing the returned IDN and baud rate.
     *         Returns an empty IDN and baud rate 0 if identification fails.
     */
    Identification getIdentification(const std::string& port);

    // -------- Thread Functions --------

    /**
     * @brief Starts the background USB device watchdog.
     *
     * Starts a std::jthread that monitors /dev for ttyUSB and ttyACM
     * device creation and deletion events.
     *
     * Calling this function while the watchdog is already running has
     * no effect.
     */
    void startWatchdog();

    /**
     * @brief Monitors /dev for serial device changes.
     *
     * Uses Linux inotify and poll() to detect serial devices being
     * connected or disconnected. Newly connected devices are identified
     * and added to the registry, while removed devices are deleted.
     *
     * @param stop Stop token used to terminate the watchdog thread.
     */
    void watchdog(std::stop_token stop);

    // -------- Read Functions --------

    /**
     * @brief Returns a copy of all registered instruments.
     *
     * The registry mutex is locked while the device map is copied.
     *
     * @return Map indexed by serial port containing registered instruments.
     */
    std::unordered_map<std::string, Instrument> readDevices();

    /**
     * @brief Returns the number of registered instruments.
     *
     * @return Number of instruments currently registered.
     */
    size_t readDeviceCount();

    // -------- Set Functions --------

    /**
     * @brief Marks an instrument as available.
     *
     * @param port Serial port of the instrument to update.
     */
    void setToAvailable(std::string& port);

    /**
     * @brief Marks an instrument as unavailable.
     *
     * @param port Serial port of the instrument to update.
     */
    void setToUnavailable(std::string& port);

private:
    std::unordered_map<std::string, Instrument> devices;

    std::jthread watchdogThread;
    std::mutex mutex;
};