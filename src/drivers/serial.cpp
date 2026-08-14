#include <cstring>
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>
#include <string>
#include <errno.h>

#include "drivers/serial.h"
#include "helper/logger/logger.h"

Serial::Serial(const std::string &port, const speed_t &baudrate)
: port(port), baudrate(baudrate) {}
 
Serial::~Serial()
{
    close();
}

bool Serial::open()
{
    fd = ::open(port.c_str(), O_RDWR | O_NOCTTY);

    if(fd == -1)
    {
        Logger::logger().log_serial()->error("Failed to open {}: {}", port, strerror(errno));
        return false;
    }
    
    if (tcgetattr(fd, &tty) != 0){
        Logger::logger().log_serial()->error("tcgetattr() failed at {}: {}", port, strerror(errno));
        ::close(fd);
        fd = -1;
        return false; 
    }

    cfsetispeed(&tty, baudrate);
    cfsetospeed(&tty, baudrate);
    
    tty.c_cflag &= ~PARENB;
    tty.c_cflag &= ~CSTOPB;
    tty.c_cflag &= ~CSIZE;
    tty.c_cflag |= CS8;
    tty.c_cflag |= CREAD | CLOCAL;
    tty.c_lflag = 0;
    tty.c_iflag = 0;
    tty.c_oflag = 0;

    tty.c_cc[VMIN] = 0;
    tty.c_cc[VTIME] = 3;

    if (tcsetattr(fd, TCSANOW, &tty) != 0)
    {
        Logger::logger().log_serial()->error("tcsetattr() failed at {}: {}", port, strerror(errno));
        ::close(fd);
        fd = -1;
        return false;
    }

    if (tcflush(fd, TCIOFLUSH) != 0)
    {
        Logger::logger().log_serial()->error("tcflush() failed at {}: {}", port, strerror(errno));
        ::close(fd);
        fd = -1;
        return false;
    }

    Logger::logger().log_serial()->trace("Opened Serial Port \"{}\" at {} baud", port, baudToInt(baudrate));
    return true;
}

bool Serial::close()
{
    if(!isOpen())
    {
        Logger::logger().log_serial()->trace("Serial Port \"{}\" is already closed",port);
        return true;
    }

    if (::close(fd) != 0)
    {
        Logger::logger().log_serial()->error("Failed to close Serial Port \"{}\": {}",port,strerror(errno));
        return false;
    }

    fd = -1;

    Logger::logger().log_serial()->trace("Closed Serial Port \"{}\"",port);
    return true;    
}

std::string Serial::read()
{
    char buffer[256];

    ssize_t bytes = ::read(fd, buffer, sizeof(buffer));

    if (bytes > 0)
    {
        return std::string(buffer, bytes);
    }

    return "";
}

std::string Serial::readLine()
{
    std::string buffer;
    char c[64];

    while (true)
    {
        ssize_t bytes = ::read(fd, c, sizeof(c));

        if (bytes < 0){
            Logger::logger().log_serial()->error("Failed to read string at \"{}\": {}", port, strerror(errno));
            break;
        }

        if (bytes == 0)
        {
            // Timeout / no more data
            break;
        }

        buffer.append(c, bytes);

        size_t pos = buffer.find('\n');
        if (pos != std::string::npos){
            buffer.resize(pos);
            if (!buffer.empty() && buffer.back() == '\r')
                buffer.pop_back();
            
            break;
        }
    }

    return buffer;
}

ssize_t Serial::write(const std::string &message)
{
    ssize_t total = 0;
    
    while (total < static_cast<ssize_t>(message.size())) //message.size() returns size_t not ssize_t
    {
        ssize_t bytes = ::write(fd, message.c_str()+total, message.size()-total);

        if (bytes <= 0)
        {
            Logger::logger().log_serial()->error("Failed to write \"{}\" to {}: {}",message, port, bytes == 0 ? "No bytes written" : strerror(errno));
            return -1;
        }
        
        total += bytes;
    }

    return total;
}

bool Serial::isOpen() const
{
    return (fd != -1);
}

int Serial::baudToInt(speed_t baudrate)
{
    switch (baudrate)
    {
        case B9600:   return 9600;
        case B19200:  return 19200;
        case B38400:  return 38400;
        case B57600:  return 57600;
        case B115200: return 115200;
        case B230400: return 230400;
        default:      return -1;
    }
}
