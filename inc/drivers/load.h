#pragma once

#include <string>
#include <vector>
#include <mutex>

#include "drivers/serial.h"

/**
 * @brief Controls a SCPI-compatible electronic load.
 *
 * Provides serial communication, channel control, setpoint configuration,
 * measurement queries, and state restoration for a multi-channel electronic
 * load.
 */
class Load
{
public:

    /**
     * @brief Constructs a Load controller.
     *
     * @param port Serial port used to communicate with the load.
     * @param baudrate Baud rate used by the serial interface.
     * @param maxChannels Maximum number of channels supported by the load.
     * @param idn Expected identification string returned by the load.
     * @param alias Human-readable name used for logging.
     */
    Load(const std::string& port,
         const speed_t& baudrate,
         int maxChannels,
         const std::string& idn = "",
         const std::string alias = "");

    /**
     * @brief Disconnects the load and destroys the controller.
     */
    ~Load();

    /**
     * @brief Stores voltage, current, and power measurements.
     */
    struct Measurements
    {
        float voltage = 0.0f;
        float current = 0.0f;
        float power = 0.0f;
    };

    /**
     * @brief Stores the desired state of a load channel.
     */
    struct State
    {
        bool outputEnabled = false;
        float current = 0.0f;
    };

    /**
     * @brief Connects to the load and verifies its identity.
     *
     * Opens the serial connection and queries the load using the SCPI
     * *IDN? command. The connection is rejected if the returned IDN
     * does not match the expected IDN.
     *
     * @return true if the connection was established and the IDN matched;
     *         false otherwise.
     */
    bool connect();

    /**
     * @brief Disables active channels and disconnects from the load.
     *
     * All currently enabled channels are disabled before the serial
     * connection is closed.
     *
     * @return true if the load was successfully disconnected;
     *         false otherwise.
     */
    bool disconnect();

    /**
     * @brief Returns whether the load is currently connected through
     * the serial interface.    

     * @return true if the load is connected;
     *         false otherwise.
     */
    bool isConnected();

    /**
     * @brief Returns the serial port used by the load.
     *
     * @return Serial port name.
     */
    const std::string& getPort() const
    {
        return serial.getPort();
    }

    /**
     * @brief Returns the expected load identification string.
     *
     * @return Expected IDN string.
     */
    const std::string& getIDN() const
    {
        return idn;
    }

    /**
     * @brief Returns whether a channel is currently enabled.
     *
     * @param channel Channel index.
     * @return true if the channel is enabled; false otherwise.
     */
    bool isEnabled(int channel)
    {
        return states[channel - 1].outputEnabled;
    }

    /**
     * @brief Changes the serial port used by the load.
     *
     * @param port New serial port name.
     */
    void setPort(const std::string& port)
    {
        serial.setPort(port);
    }

    /**
     * @brief Changes the expected load identification string.
     *
     * @param idn New expected IDN string.
     */
    void setIDN(const std::string& idn)
    {
        this->idn = idn;
    }

    /**
     * @brief Enables a load channel.
     *
     * @param channel Channel number, starting from 1.
     * @return true if the channel was successfully enabled;
     *         false otherwise.
     */
    bool enable(int channel);

    /**
     * @brief Disables a load channel.
     *
     * @param channel Channel number, starting from 1.
     * @return true if the channel was successfully disabled;
     *         false otherwise.
     */
    bool disable(int channel);

    /**
     * @brief Sets the current setpoint of a load channel.
     *
     * @param current Current setpoint in amperes.
     * @param channel Channel number, starting from 1.
     * @return true if the current was successfully set;
     *         false otherwise.
     */
    bool setCurrent(float current, int channel);

    /**
     * @brief Queries the identification string of the load.
     *
     * @return Load IDN string, or an empty string if the query fails.
     */
    std::string queryIDN();

    /**
     * @brief Queries the configured output state of a channel.
     *
     * @param channel Channel number, starting from 1.
     * @return 1 if enabled, 0 if disabled, or -1 if the query fails.
     */
    int queryState(int channel);

    /**
     * @brief Queries the configured voltage setpoint of a channel.
     *
     * @param channel Channel number, starting from 1.
     * @return Voltage setpoint in volts, or -1 if the query fails.
     */
    float queryVoltage(int channel);

    /**
     * @brief Queries the configured current setpoint of a channel.
     *
     * @param channel Channel number, starting from 1.
     * @return Current setpoint in amperes, or -1 if the query fails.
     */
    float queryCurrent(int channel);

    /**
     * @brief Measures the actual voltage of a channel.
     *
     * @param channel Channel number, starting from 1.
     * @return Measured voltage in volts, or -1 if the measurement fails.
     */
    float measureVoltage(int channel);

    /**
     * @brief Measures the actual current of a channel.
     *
     * @param channel Channel number, starting from 1.
     * @return Measured current in amperes, or -1 if the measurement fails.
     */
    float measureCurrent(int channel);

    /**
     * @brief Measures all available electrical quantities for a channel.
     *
     * Queries the load for voltage, current, and power measurements.
     *
     * @param channel Channel number, starting from 1.
     * @return Measurements containing voltage, current, and power.
     *         Returns an empty structure if the measurement fails.
     */
    Measurements measureAll(int channel);

    /**
     * @brief Restores the desired state of all load channels.
     *
     * Queries the current state of each channel and reapplies the stored
     * current setpoint and output state when they differ.
     *
     * @return true if all channel states were successfully restored;
     *         false if restoration failed.
     */
    bool restoreState();

private:

    std::string idn;
    std::string alias;

    const int maxChannels;

    /**
     * @brief Sends a SCPI query and returns the response.
     *
     * @param command SCPI query command.
     * @return Response from the load, or an empty string on failure.
     */
    std::string query(const std::string& command);

    /**
     * @brief Writes a SCPI command to the load.
     *
     * @param command SCPI command to send.
     * @return true if the command was successfully written;
     *         false otherwise.
     */
    bool write(const std::string& command);

    /**
     * @brief Selects the active load channel.
     *
     * @param channel Channel number, starting from 1.
     * @return true if the channel was successfully selected;
     *         false otherwise.
     *
     * @note This function does not acquire the mutex and is intended
     *       to be called by functions that already hold the mutex.
     */
    bool setChannel(int channel);

    std::vector<State> states;

    std::mutex mutex;

    Serial serial;
};