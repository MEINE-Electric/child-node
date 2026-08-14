#pragma once

#include <chrono>
#include <string>

#include "drivers/load.h"
#include "drivers/supply.h"
#include "drivers/module.h"
#include "helper/helper.h"

class Channel
{
public:
    // -------- Constructor & Deconstructor -------- 
    Channel(Load& load, Supply& supply, Module& module, int channel, std::string channelID);
    ~Channel();

    // -------- Helper Structs -------- 
    struct data
    {
        int channelNumber;
        std::string state;
        
        float loadVoltage;
        float loadCurrent;
        float supplyVoltage;
        float supplyCurrent;

        std::string loadState;
        std::string supplyState;
        std::string moduleState;

        int elapsedTime;
        int remainingTime;
        int totalTime;

        int userStepCount;
        int generatedStepCount;
        int loopCount;
    };
    
    // -------- Execution & Updation -------- 
    void run();
    void execute(const Charge& cmd);
    void execute(const Discharge& cmd);
    void execute(const Rest& cmd);
    void execute(const Hold& cmd);
    void execute(const Goto& cmd);

    // -------- Control commands -------- 
    void startExperiment();
    void pauseExperiment();
    void resumeExperiment();
    void stopExperiment();
    void skipStep();

    // -------- Experiment transactions -------- 
    bool updateExperiment(const std::vector<Command>& experiment);
    void deleteExperiment();

    // -------- Logging --------
    std::string returnDevices();

    // -------- Getter -------
    std::string getChannelID() {return channelID;}
    bool isRunning() {return running; }
    
private:
    std::string channelID;

    Load& load;
    Supply& supply;
    Module& module;
    int loadChannel;

    std::vector<Command> experiment;

    enum class State
    {
        Idle,
        Charging,
        Discharging,
        Holding,
        Resting,
        Finished,
        Error
    };

    // -------- Mutable state for the active experiment --------
    struct RuntimeState
    {
        State state = State::Idle;
        std::size_t currentStep = 0;
        int currentLoop = 0; // Increments on each successful GOTO loop

        std::chrono::steady_clock::time_point startTimePoint{};
        int timeElapsed = 0;
        int totalDuration = 0;
        float thresholdVoltage = 0.0f;

        float supplyVoltage = 0.0f;
        float supplyCurrent = 0.0f;
        bool supplyEnabled = false;

        float loadVoltage = 0.0f;
        float loadCurrent = 0.0f;
        bool loadEnabled = false;

        Module::State moduleState = Module::State::OFF;
    } runtime;

    // -------- Channel Status --------
    bool ready = false; 
    bool running = false;

    // -------- Polling Function --------
    void polling();

    // -------- Experiment Stop --------
    void finishExperiment();
    void nextStep();
};
