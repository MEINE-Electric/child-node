#include <cstddef>
#include <filesystem>
#include <mutex>
#include <stop_token>
#include <termios.h>
#include <thread>
#include <chrono>
#include <sys/inotify.h>
#include <unistd.h>
#include <poll.h>
#include <unordered_map>

#include "helper/logger/logger.h"
#include "registry/registry.h"
#include "drivers/serial.h"

bool Registry::initialScan() // Run to create an initial field
{
    for (const std::filesystem::directory_entry& entry : std::filesystem::directory_iterator("/dev"))
    {
        const std::string name = entry.path().filename().string();

        if (name.starts_with("ttyUSB"))
        {
            Logger::logger().log_registry()->info("Found serial port: {}", entry.path().string());
            
            Identification identity = getIdentification(entry.path().string()); 
            {
                std::lock_guard<std::mutex> lock(mutex);
                devices[entry.path().string()] = Instrument({.idn = identity.idn, .port = entry.path().string(),.baudrate = identity.baudrate,.available = true});
            }
        }
    }

    Logger::logger().log_registry()->info("Discovered {} serial port(s)", devices.size());
    return !devices.empty();
}

Registry::Identification Registry::getIdentification(const std::string& port)
{
    constexpr speed_t baudrates[] = {
        B9600,
        B19200,
        B38400,
        B57600,
        B115200
    };

    constexpr int MAX_RETRIES = 1;

    for (speed_t baudrate : baudrates)
    {
        for (int attempt = 1; attempt <= MAX_RETRIES; ++attempt)
        {
            Serial serial(port, baudrate);

            if (!serial.open())
                continue;

            std::this_thread::sleep_for(std::chrono::seconds(2));

            if (serial.write("*IDN?\n") >= 0)
            {
                std::string response = serial.readLine();

                if (!response.empty())
                {
                    serial.close();

                    Logger::logger().log_registry()->info(
                        "Found '{}' on {}",
                        response,
                        port);

                    return {response, baudrate};
                }
            }

            std::this_thread::sleep_for(std::chrono::seconds(2));
            serial.close();
        }
    }

    return {"", 0};
}

void Registry::startWatchdog()
{
    if (watchdogThread.joinable())
        return;

    watchdogThread = std::jthread([this](std::stop_token stop) 
    {
        watchdog(stop);
    });
}

void Registry::watchdog(std::stop_token stop)
{
    // Monitor the /dev filesystem
    int fd = inotify_init1(IN_CLOEXEC);
    if (fd == -1)
    {
        Logger::logger().log_registry()->error("Failure to initialise inotify");
        return;
    }
    int wd = inotify_add_watch(fd,"/dev",IN_CREATE | IN_DELETE );    
    if (wd == -1)
    {
        Logger::logger().log_registry()->error("Failure to add watch to inotify");
        close(fd);
        return;
    }

    // Use polling for non-blocking
    struct pollfd pfd;
    pfd.fd = fd;
    pfd.events = POLLIN;

    char buffer[4096];

    while (!stop.stop_requested())
    {
        int ret = poll(&pfd, 1, 500);

        if (ret == -1)
        {
            Logger::logger().log_registry()->error("poll() failed: {}",strerror(errno));
            break;
        }

        if (ret == 0)
        {
            continue;
        }

        if (pfd.revents & POLLIN){
            ssize_t bytes = read(fd, buffer, sizeof(buffer));

            if (bytes <= 0)
                continue;

            size_t offset = 0;

            while (offset < static_cast<size_t>(bytes))
            {
                auto* event = reinterpret_cast<inotify_event*>(buffer + offset);
                if (event->len > 0)
                {
                    std::string name(event->name); 
                    if (!name.starts_with("ttyUSB") && !name.starts_with("ttyACM"))
                    {
                        offset += sizeof(inotify_event) + event->len;
                        continue;
                    }

                    if (event->mask & IN_CREATE){
                        Logger::logger().log_registry()->debug("USB Device connected: {}", name);
                        
                        std::this_thread::sleep_for(std::chrono::milliseconds(500)); // Wait for kernel init

                        std::string port = "/dev/" + name;
                        Identification identity = getIdentification(port);
                        {
                            std::lock_guard<std::mutex> lock(mutex);
                            Instrument tempDevice = {.idn=identity.idn, .port=port, .baudrate=identity.baudrate, .available=true};
                            devices[port] = tempDevice;
                        }

                        deviceAdded = true;
                        cv.notify_one();
                    }
                    if (event->mask & IN_DELETE){
                        std::lock_guard<std::mutex> lock(mutex);
                        Logger::logger().log_registry()->debug("USB Device disconnected: {}", name);

                        std::string port = "/dev/" + name;
                        devices.erase(port);
                    }
                }
                offset += sizeof(inotify_event) + event->len;
            }
        }
    }
    inotify_rm_watch(fd, wd);
    close(fd);
}

size_t Registry::readDeviceCount()
{
    std::lock_guard<std::mutex> lock(mutex);
    return devices.size();
}

std::unordered_map<std::string, Registry::Instrument> Registry::readDevices()
{
    std::lock_guard<std::mutex> lock(mutex);
    return devices;
}

void Registry::setToAvailable(std::string& port)
{
    std::lock_guard<std::mutex> lock(mutex);
    devices[port] = Instrument({.idn=devices[port].idn, .port=port, .baudrate=devices[port].baudrate, .available=true});
}

void Registry::setToUnavailable(std::string& port)
{
    std::lock_guard<std::mutex> lock(mutex);
    devices[port] = Instrument({.idn=devices[port].idn, .port=port, .baudrate=devices[port].baudrate, .available=false});
}

std::string Registry::findDeviceByIDN(const std::string& idn)
{
    std::lock_guard<std::mutex> lock(mutex);
    for (const auto& [port, device]: devices)
    {
        if (device.idn == idn)
            return port;
    }

    return "";
}

void Registry::waitForDevice()
{
    std::unique_lock<std::mutex> lock(mutex);

    // Wait temporarily releases the lock and reacquires it after being notified
    cv.wait(lock, [this] { 
        return deviceAdded;
    });

    deviceAdded = false;
}