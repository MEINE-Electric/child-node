#pragma once

#include <fcntl.h>
#include <termios.h>
#include <unistd.h>
#include <string>

class Serial
{
public:
    Serial(const std::string &port, const speed_t &baudrate);
    ~Serial();
    
    // -------- Open and close serial ports --------
    bool open();
    bool close();

    // -------- R/W functions for the serial port --------
    std::string read();
    std::string readLine();
    ssize_t write(const std::string &message);
   
    // -------- Status check function --------
    bool isOpen() const;
    
    // -------- Basic Setters/Getters --------
    const std::string& getPort() const { return port; }
    speed_t getBaudrate() const { return baudrate; }
    void setPort(const std::string& port) { this->port = port; }
private:
    int fd = -1;
    termios tty{};
    std::string port;
    speed_t baudrate;
    
    static int baudToInt(speed_t baud);
};