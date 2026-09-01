#pragma once

#include <string>

#include "drivers/serial.h"

/**
 * @brief Controls a SCPI-compatible programmable power supply.
 *
 * Provides serial communication, output control, voltage and current
 * configuration, measurement queries, and state restoration.
 */
class Supply
{
public:

    /**
     * @brief Constructs a Supply controller.
     *
     * The serial connection is not opened automatically. Call connect()
     * to establish communication with the supply.
     *
     * @param port Serial port used to communicate with the supply.
     * @param baudrate Baud rate used by the serial interface.
     * @param idn Expected identification string returned by the supply.
     * @param alias Human-readable name used for logging.
     */
    Supply(const std::string& port,
           const speed_t& baudrate,
           const std::string& idn = "",
           const std::string alias = "");

    /**
     * @brief Disconnects the supply when the controller is destroyed.
     *
     * If the supply output is enabled, it is disabled before the
     * serial connection is closed.
     */
    ~Supply();

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
     * @brief Stores the desired state of the power supply.
     *
     * Contains the software-tracked output state, voltage setpoint,
     * and current setpoint.
     */
    struct State
    {
        bool outputEnabled = false;
        float voltage = 0.0f;
        float current = 0.0f;
    };

    /**
     * @brief Connects to the power supply and verifies its identity.
     *
     * Opens the serial connection and queries the supply using the
     * SCPI *IDN? command. If an expected IDN was configured, the
     * returned IDN must match it for the connection to succeed.
     *
     * @return true if the connection was established and the IDN
     *         matched; false otherwise.
     */
    bool connect();

    /**
     * @brief Disables the supply output and disconnects.
     *
     * If the output is currently enabled, it is disabled before the
     * serial connection is closed.
     *
     * @return true if the supply was successfully disconnected;
     *         false otherwise.
     */
    bool disconnect();

    /**
     * @brief Returns the serial port used by the supply.
     *
     * @return Serial port name.
     */
    const std::string& getPort() const
    {
        return serial.getPort();
    }

    /**
     * @brief Returns whether the load is currently connected through
     * the serial interface.    

     * @return true if the load is connected;
     *         false otherwise.
     */
    bool isConnected();

    /**
     * @brief Returns the expected supply identification string.
     *
     * @return Expected IDN string.
     */
    const std::string& getIDN() const
    {
        return idn;
    }

    /**
     * @brief Returns the cached output state.
     *
     * This value represents the state most recently commanded through
     * enable() or disable(). It does not query the hardware.
     *
     * @return true if the software state indicates that the output
     *         is enabled; false otherwise.
     */
    bool isEnabled() const
    {
        return state.outputEnabled;
    }

    /**
     * @brief Changes the serial port used by the supply.
     *
     * The new port is used the next time connect() is called.
     *
     * @param port New serial port name.
     */
    void setPort(const std::string& port)
    {
        this->serial.setPort(port);
    }

    /**
     * @brief Changes the expected supply identification string.
     *
     * @param idn New expected IDN string.
     */
    void setIDN(const std::string& idn)
    {
        this->idn = idn;
    }

    /**
     * @brief Enables the supply output.
     *
     * Sends the SCPI OUTP ON command and updates the cached output
     * state if the command succeeds.
     *
     * @return true if the output was successfully enabled;
     *         false otherwise.
     */
    bool enable();

    /**
     * @brief Disables the supply output.
     *
     * Sends the SCPI OUTP OFF command and updates the cached output
     * state if the command succeeds.
     *
     * @return true if the output was successfully disabled;
     *         false otherwise.
     */
    bool disable();

    /**
     * @brief Sets the output voltage.
     *
     * Sends the SCPI VOLT command and updates the cached voltage
     * setpoint if the command succeeds.
     *
     * @param voltage Voltage setpoint in volts.
     * @return true if the voltage was successfully set;
     *         false otherwise.
     */
    bool setVoltage(float voltage);

    /**
     * @brief Sets the output current limit.
     *
     * Sends the SCPI CURR command and updates the cached current
     * setpoint if the command succeeds.
     *
     * @param current Current setpoint in amperes.
     * @return true if the current was successfully set;
     *         false otherwise.
     */
    bool setCurrent(float current);

    /**
     * @brief Queries the identification string from the supply.
     *
     * Sends the SCPI *IDN? command to the hardware.
     *
     * @return Hardware-reported IDN string, or an empty string if
     *         the query fails.
     */
    std::string queryIDN();

    /**
     * @brief Queries the output state reported by the supply.
     *
     * Sends the SCPI OUTP? command to the hardware.
     *
     * @return 1 if the output is enabled, 0 if disabled, or -1 if
     *         the query fails.
     */
    int queryState();

    /**
     * @brief Queries the configured voltage setpoint.
     *
     * Sends the SCPI VOLT? command to the hardware.
     *
     * @return Configured voltage in volts, or -1 if the query fails.
     */
    float queryVoltage();

    /**
     * @brief Queries the configured current setpoint.
     *
     * Sends the SCPI CURR? command to the hardware.
     *
     * @return Configured current in amperes, or -1 if the query fails.
     */
    float queryCurrent();

    /**
     * @brief Measures the actual output voltage.
     *
     * Sends the SCPI MEAS:VOLT? command to the hardware.
     *
     * @return Measured voltage in volts, or -1 if the measurement fails.
     */
    float measureVoltage();

    /**
     * @brief Measures the actual output current.
     *
     * Sends the SCPI MEAS:CURR? command to the hardware.
     *
     * @return Measured current in amperes, or -1 if the measurement fails.
     */
    float measureCurrent();

    /**
     * @brief Measures the output voltage, current, and power.
     *
     * Sends the SCPI MEAS:ALL? command and parses the returned
     * comma-separated measurement values.
     *
     * @return Measurements containing voltage, current, and power.
     *         Returns an empty structure if the query or parsing fails.
     */
    Measurements measureAll();

    /**
     * @brief Restores the previously stored supply state.
     *
     * Queries the hardware voltage, current, and output state and
     * reapplies any values that differ from the cached software state.
     *
     * A small floating-point tolerance is used when comparing voltage
     * and current setpoints.
     *
     * @return true if the supply state was successfully verified or
     *         restored; false if any state could not be queried or set.
     */
    bool restoreState();

private:

    std::string idn;
    std::string alias;

    /**
     * @brief Sends a SCPI query to the supply.
     *
     * @param command SCPI query command.
     * @return Response from the supply, or an empty string if the
     *         query fails.
     */
    std::string query(const std::string& command);

    /**
     * @brief Writes a SCPI command to the supply.
     *
     * @param command SCPI command to send.
     * @return true if the command was successfully written;
     *         false otherwise.
     */
    bool write(const std::string& command);

    State state{};

    Serial serial;
};