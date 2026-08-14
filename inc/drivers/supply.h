#pragma once

#include <string>

#include "drivers/serial.h"

class Supply
{
public:
    Supply(const std::string &port, const speed_t &baudrate, const std::string &idn = "", const std::string alias = "");
    ~Supply();

    // -------- Helper Structs --------
    struct Measurements
    {
        float voltage = 0.0f;
        float current = 0.0f;
        float power = 0.0f;
    };

    struct State
    {
        bool outputEnabled = false;
        float voltage = 0.0f;
        float current = 0.0f;
    };

    // -------- Open and close serial ports --------
    bool connect();
    bool disconnect();

    // -------- Basic Setters & Getters --------
    const std::string& getPort() const { return serial.getPort(); }
    const std::string& getIDN() const { return idn; }
    const bool isEnabled() { return state.outputEnabled; }
    void setPort(const std::string& port) { this->serial.setPort(port); }
    void setIDN(const std::string& idn) { this->idn = idn; }

    // -------- R/W code to the instrument --------
    bool enable();                                      // Turn on the load
    bool disable();                                     // Turn off the load
    bool setVoltage(float voltage);                     // Set voltage
    bool setCurrent(float current);                     // Set current

    std::string queryIDN();                             // Read instrument idn
    int queryState();                                   // Read set state
    float queryVoltage();                               // Read set voltage
    float queryCurrent();                               // Read set current

    float measureVoltage();                             // Read measured voltage
    float measureCurrent();                             // Read measured current
    Measurements measureAll();                          // Read all measured 
    
    // -------- Error/Fault Handling --------
    bool restoreState();                                // Reapply the desired state when it differs
private:
    std::string idn;
    std::string alias;

    std::string query(const std::string &command); 
    bool write(const std::string &command);

    State state{};
    Serial serial;
};
