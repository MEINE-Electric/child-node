#pragma once

#include <chrono>
#include <condition_variable>
#include <mutex>
#include <queue>
#include <stop_token>
#include <string>
#include <thread>

#include "drivers/load.h"
#include "drivers/supply.h"
#include "drivers/module.h"
#include "helper/helper.h"
#include "event/event.h"

class Channel
{
public:
    // -------- Constructor & Deconstructor -------- 
    Channel(Load& load, Supply& supply, Module& module, int channel, std::string channelID, ChannelEventBus* eventBus = nullptr);
    ~Channel();

    // -------- Helper Structs -------- 
    struct Data
    {
        std::string channelNumber;
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

        int currentStep;
        int loopCount;

        nlohmann::json toJson() const;
    };

    // -------- Control commands -------- 
    void enqueueCommand(const std::string& command);

    // -------- Experiment transactions -------- 
    bool updateExperiment(const std::vector<Command>& experiment);
    void deleteExperiment();

    // -------- Logging --------
    std::string returnDevices() const;
    Data getLatestData() const;

    //  -------- Thread Functions --------
    void startWorkerThread();
    void stopWorkerThread();

    // -------- Getter -------
    std::string getChannelID() const {return channelID;}
    bool isInProcess() const {return inProcess; }
    
private:
    std::string channelID;

    Load& load;
    Supply& supply;
    Module& module;
    ChannelEventBus* eventBus;
    int loadChannel;

    std::vector<Command> experiment;
    std::queue<std::string> controlQueue;

    std::jthread workerThread;

    enum class State
    {
        Idle,
        Charging,
        Discharging,
        Holding,
        Resting,
        Finished,
        Error,
        Paused
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
    bool inProcess = false;

    // -------- Polling Function --------
    void polling();

    // -------- Execution & Updation -------- 
    void run(std::stop_token stop);
    void execute(const Charge& cmd);
    void execute(const Discharge& cmd);
    void execute(const Rest& cmd);
    void execute(const Hold& cmd);
    void execute(const Goto& cmd);

    // -------- Experiment Controls --------
    bool startExperiment();
    bool pauseExperiment();
    bool resumeExperiment();
    bool stopExperiment();
    bool skipStep();
    void finishExperiment();
    void nextStep();

    // -------- Thread Controls --------
    std::mutex controlMutex;
    std::condition_variable controlCV;
    void checkControlQueue();
    void waitForControl(std::stop_token& stop);

    // -------- Helper Functions --------
    void connectAllDevices();
    void disconnectAllDevices();
    std::string stateToString(State state) const;
};
