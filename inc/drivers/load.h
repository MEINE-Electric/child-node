#pragma once

#include <string>
#include <vector>
#include <mutex>

#include "drivers/serial.h"

class Load{
public:
    Load(const std::string &port, const speed_t &baudrate, int maxChannels, const std::string &idn = "", const std::string alias = "");
    ~Load();

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
        float current = 0.0f;
    };

    // -------- Open and close serial ports --------
    bool connect();
    bool disconnect();

    // -------- Basic Setters & Getters --------
    const std::string& getPort() const { return serial.getPort(); }
    const std::string& getIDN() const { return idn; }
    bool isEnabled(int channel) { return states[channel].outputEnabled; }
    void setPort(const std::string& port) { this->serial.setPort(port); }
    void setIDN(const std::string& idn) { this->idn = idn; }

    // -------- R/W code to the instrument --------
    bool enable(int channel);                           // Turn on the load
    bool disable(int channel);                          // Turn off the load
    bool setCurrent(float current, int channel);        // Set current to channel

    std::string queryIDN();                             // Read instrument idn
    int queryState(int channel);                        // Read set state at channel
    float queryVoltage(int channel);                    // Read set voltage at channel
    float queryCurrent(int channel);                    // Read set current at channel

    float measureVoltage(int channel);                  // Read measured voltage at channel
    float measureCurrent(int channel);                  // Read measured current at channel
    Measurements measureAll(int channel);               // Read all measured values at channel
    
    // -------- Error/Fault Handling --------
    bool restoreState();                                // Reapply the desired state when it differs
private:
    std::string idn;
    std::string alias;
    const int maxChannels;

    std::string query(const std::string &command); 
    bool write(const std::string &command);
    bool setChannel(int channel);                       // Should not hold mutex or called by public users
    
    std::vector<State> states;                          // Vector holds previous states
    std::mutex mutex;                                   // Mutex for exclusive R/W access between channels
    Serial serial;
};
