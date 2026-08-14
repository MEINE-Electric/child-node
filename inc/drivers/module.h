#pragma once

#include <string>

#include "drivers/serial.h"

class Module{
public:
    enum State
    {
        OFF,
        DISCHARGE,
        CHARGE,
        ERROR
    }; 

    Module(const std::string &port, const speed_t &baudrate, const std::string &idn = "", const std::string alias = "");
    ~Module();

    // -------- Open and close serial ports --------
    bool connect();
    bool disconnect();

    // -------- Basic Setters & Getters --------
    const std::string& getPort() const { return serial.getPort(); }
    const std::string& getIDN() const { return idn; }
    void setPort(const std::string& port) { this->serial.setPort(port); }
    void setIDN(const std::string& idn) { this->idn = idn; }
    State getState() const { return state; }

    // -------- R/W code to the instrument --------
    bool setToCharge();                                 // Set to charging mode
    bool setToDischarge();                              // Set to discharging mode
    bool setToOff();                                    // Set to safe mode

    std::string queryIDN();                             // Read instrument idn
    State queryState();                           // Read instrument state (as per the hardware)
private:
    std::string idn;
    std::string alias;

    std::string query(const std::string &command); 
    bool write(const std::string &command);

    Serial serial;
    State state;
};