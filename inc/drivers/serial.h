#pragma once

#include <fcntl.h>
#include <termios.h>
#include <unistd.h>
#include <string>

/**
 * @brief Provides a Linux serial-port interface.
 *
 * Handles opening, configuring, reading from, writing to, and closing
 * a serial port using the POSIX termios API.
 *
 * The serial interface is configured for 8 data bits, no parity,
 * one stop bit, and local read/write operation.
 */
class Serial
{
public:

    /**
     * @brief Constructs a serial-port controller.
     *
     * The serial port is not opened automatically. Call open() to
     * establish the connection.
     *
     * @param port Serial device path, for example "/dev/ttyUSB0".
     * @param baudrate POSIX baud-rate constant, for example B9600.
     */
    Serial(const std::string& port, const speed_t& baudrate);

    /**
     * @brief Closes the serial port when the controller is destroyed.
     */
    ~Serial();

    /**
     * @brief Opens and configures the serial port.
     *
     * Opens the configured device and applies the configured baud rate
     * and serial communication settings.
     *
     * The port is configured for 8 data bits, no parity, one stop bit,
     * with CREAD and CLOCAL enabled. Reads use a timeout rather than
     * blocking indefinitely.
     *
     * @return true if the port was successfully opened and configured;
     *         false otherwise.
     */
    bool open();

    /**
     * @brief Closes the serial port.
     *
     * If the port is already closed, this function succeeds without
     * performing any operation.
     *
     * @return true if the port was successfully closed or was already
     *         closed; false if the close operation failed.
     */
    bool close();

    /**
     * @brief Reads available data from the serial port.
     *
     * Attempts to read up to 256 bytes from the serial port.
     *
     * @return Data read from the serial port, or an empty string if
     *         no data was received or the read failed.
     */
    std::string read();

    /**
     * @brief Reads a line of data from the serial port.
     *
     * Reads data until a newline character is received or the configured
     * serial read timeout expires. The newline and an optional carriage
     * return are removed from the returned string.
     *
     * @return Line received from the serial port without the newline
     *         terminator, or an empty string if no data was received.
     */
    std::string readLine();

    /**
     * @brief Writes a complete message to the serial port.
     *
     * Continues writing until all bytes in the message have been sent
     * or a write operation fails.
     *
     * @param message Data to write to the serial port.
     * @return Number of bytes written, or -1 if the write failed.
     */
    ssize_t write(const std::string& message);

    /**
     * @brief Checks whether the serial port is currently open.
     *
     * @return true if the serial port is open; false otherwise.
     */
    bool isOpen() const;

    /**
     * @brief Returns the configured serial port.
     *
     * @return Serial device path.
     */
    const std::string& getPort() const
    {
        return port;
    }

    /**
     * @brief Returns the configured baud rate.
     *
     * @return POSIX baud-rate constant.
     */
    speed_t getBaudrate() const
    {
        return baudrate;
    }

    /**
     * @brief Changes the serial port.
     *
     * The new port will be used the next time open() is called.
     *
     * @param port New serial device path.
     */
    void setPort(const std::string& port)
    {
        this->port = port;
    }

private:

    int fd = -1;
    termios tty{};
    std::string port;
    speed_t baudrate;

    /**
     * @brief Converts a POSIX baud-rate constant to its numeric value.
     *
     * @param baud Baud-rate constant such as B9600.
     * @return Numeric baud rate, or -1 if the baud rate is unsupported.
     */
    static int baudToInt(speed_t baud);
};