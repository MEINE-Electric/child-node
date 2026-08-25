#pragma once

#include <string>

#include "drivers/serial.h"

/**
 * @brief Controls ORCA module through a serial interface.
 *
 * Provides serial communication, operating-state control, identification,
 * and hardware-state querying for the module.
 */
class Module
{
public:

    /**
     * @brief Represents the operating state of the module.
     */
    enum State
    {
        OFF,
        DISCHARGE,
        CHARGE,
        ERROR
    };

    /**
     * @brief Constructs a Module controller.
     *
     * Initializes the serial interface and sets the software state
     * to OFF.
     *
     * @param port Serial port used to communicate with the module.
     * @param baudrate Baud rate used by the serial interface.
     * @param idn Expected identification string returned by the module.
     * @param alias Human-readable name used for logging.
     */
    Module(const std::string& port,
           const speed_t& baudrate,
           const std::string& idn = "",
           const std::string alias = "");

    /**
     * @brief Disconnects the module and destroys the controller.
     *
     * The module is placed in the OFF state before the serial connection
     * is closed.
     */
    ~Module();

    /**
     * @brief Opens the serial connection to the module.
     *
     * Gives the module time to reboot after the serial connection is
     * opened. If an expected IDN was provided, the module is queried
     * and the returned IDN is verified.
     *
     * @return true if the connection was established and the IDN matched;
     *         false otherwise.
     */
    bool connect();

    /**
     * @brief Turns the module off and closes the serial connection.
     *
     * If the module is currently in CHARGE or DISCHARGE state, it is
     * first commanded to enter the OFF state.
     *
     * @return true if the module was successfully disconnected;
     *         false otherwise.
     */
    bool disconnect();

    /**
     * @brief Returns the serial port used by the module.
     *
     * @return Serial port name.
     */
    const std::string& getPort() const
    {
        return serial.getPort();
    }

    /**
     * @brief Returns the expected module identification string.
     *
     * @return Expected IDN string.
     */
    const std::string& getIDN() const
    {
        return idn;
    }

    /**
     * @brief Changes the serial port used by the module.
     *
     * @param port New serial port name.
     */
    void setPort(const std::string& port)
    {
        this->serial.setPort(port);
    }

    /**
     * @brief Changes the expected module identification string.
     *
     * @param idn New expected IDN string.
     */
    void setIDN(const std::string& idn)
    {
        this->idn = idn;
    }

    /**
     * @brief Returns the module's cached software state.
     *
     * This value represents the state most recently commanded through
     * setToCharge(), setToDischarge(), or setToOff(). It does not query
     * the hardware.
     *
     * @return Cached module state.
     */
    State getState() const
    {
        return state;
    }

    /**
     * @brief Sets the module to charging mode.
     *
     * Sends the STATE CHARGE command to the module and updates the
     * cached software state if the command succeeds.
     *
     * @return true if the module was successfully commanded to charge;
     *         false otherwise.
     */
    bool setToCharge();

    /**
     * @brief Sets the module to discharging mode.
     *
     * Sends the STATE DISCHARGE command to the module and updates the
     * cached software state if the command succeeds.
     *
     * @return true if the module was successfully commanded to discharge;
     *         false otherwise.
     */
    bool setToDischarge();

    /**
     * @brief Sets the module to the safe OFF state.
     *
     * Sends the STATE OFF command to the module and updates the cached
     * software state if the command succeeds.
     *
     * @return true if the module was successfully turned off;
     *         false otherwise.
     */
    bool setToOff();

    /**
     * @brief Queries the identification string from the hardware.
     *
     * Sends the SCPI *IDN? command and returns the response from the
     * module.
     *
     * @return Hardware-reported IDN string, or an empty string if the
     *         query fails.
     */
    std::string queryIDN();

    /**
     * @brief Queries the current state reported by the hardware.
     *
     * Sends the STATE? command and converts the returned string into
     * the corresponding State value.
     *
     * Unlike getState(), this function communicates with the hardware
     * and does not update the cached software state.
     *
     * @return Hardware-reported state, or State::ERROR if the response
     *         is invalid or the query fails.
     */
    State queryState();

private:

    std::string idn;
    std::string alias;

    /**
     * @brief Sends a command to the module and returns its response.
     *
     * @param command Command to send to the module.
     * @return Response received from the module, or an empty string if
     *         the query fails.
     */
    std::string query(const std::string& command);

    /**
     * @brief Writes a command to the module.
     *
     * @param command Command to send.
     * @return true if the command was successfully written;
     *         false otherwise.
     */
    bool write(const std::string& command);

    Serial serial;

    State state;
};