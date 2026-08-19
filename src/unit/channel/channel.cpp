#include <chrono>
#include <stop_token>
#include <thread>

#include "drivers/load.h"
#include "drivers/supply.h"
#include "drivers/module.h"
#include "helper/logger/logger.h"
#include "unit/channel/channel.h"

Channel::Channel(Load& load, Supply& supply, Module& module, int channel, std::string channelID)
: load(load), supply(supply), module(module), loadChannel(channel), channelID(channelID)
{
}

Channel::~Channel()
{    
    ready = false;
    stopWorkerThread();
}

// Public Functions
bool Channel::updateExperiment(const std::vector<Command>& experiment)
{
    if (running)
    {
        return false;
    }

    this->experiment = std::move(experiment);
    ready = true;
    return true;
}

void Channel::deleteExperiment()
{
    if(!running)
    {
        experiment.clear();
        ready = false;
    }
}

std::string Channel::returnDevices()
{
    std::string devices = "Channel:" + channelID + "\n" + "Load:" + load.getIDN() + "\n" + "Supply:" + supply.getIDN() + "\n" + "Module:" + module.getIDN();
    return devices;
}

Channel::Data Channel::getLatestData()
{
    data.channelNumber = channelID;
    data.state = stateToString(runtime.state);

    data.loadVoltage = runtime.loadVoltage;
    data.loadCurrent = runtime.loadCurrent;
    data.supplyVoltage = runtime.supplyVoltage;
    data.supplyCurrent = runtime.supplyCurrent;

    data.loadState = runtime.loadEnabled ? "on" : "off";
    data.supplyState = runtime.supplyEnabled ? "on" : "off";
    data.moduleState = runtime.moduleState == Module::State::OFF ? "off" :
                        runtime.moduleState == Module::State::DISCHARGE ? "discharge" :
                        runtime.moduleState == Module::State::CHARGE ? "charge" : "error";
    
    data.elapsedTime = runtime.timeElapsed;
    data.remainingTime = runtime.totalDuration - runtime.timeElapsed;
    data.totalTime = runtime.totalDuration;

    data.currentStep = runtime.currentStep;
    data.loopCount = runtime.currentLoop;

    return data;
}

void Channel::enqueueCommand(const std::string& command)
{
    {
        std::lock_guard lock(controlMutex);
        controlQueue.push(command);
    }

    controlCV.notify_one();
}

void Channel::startWorkerThread()
{
    if (workerThread.joinable())
    {
        Logger::logger().log_channel()->warn("Channel-{} worker thread is already running",channelID);
        return;
    }
    workerThread = std::jthread([this](std::stop_token stop)
    {
        run(stop);
    });
    Logger::logger().log_channel()->info("Channel-{} worker thread started",channelID);
}

void Channel::stopWorkerThread()
{
    if (workerThread.joinable())
    {
        workerThread.request_stop();
        workerThread.join();
        Logger::logger().log_channel()->info("Channel-{} worker thread stopped",channelID);
    }
}

// Private Functions
void Channel::run(std::stop_token stop)
{
    while (!stop.stop_requested())
    {
        // Control command check
        checkControlQueue();
        if (!running)
        {
            waitForControl(stop);
            continue;
        }  

        // Polling Block
        if(runtime.state != State::Idle && runtime.state != State::Finished && running)
        {
            polling();
            runtime.timeElapsed = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - runtime.startTimePoint).count();
        }
        
        // Condition check block
        if(running && runtime.state == State::Idle)
        {
            std::visit([this](const auto& cmd)
            {
                execute(cmd);
            }, experiment[runtime.currentStep]);
        }
        else if(runtime.state != State::Paused) // Running State
        {
            if(runtime.timeElapsed >= runtime.totalDuration)
            {
                Logger::logger().log_channel()->debug(
                    "Channel {} ending step {}: duration limit reached ({} / {} s)",
                    channelID, runtime.currentStep, runtime.timeElapsed, runtime.totalDuration);
                nextStep();
            }
            else if (runtime.state == State::Charging && runtime.thresholdVoltage <= runtime.supplyVoltage)
            {
                Logger::logger().log_channel()->debug(
                    "Channel {} ending charge step: supply voltage {:.3f} V reached cutoff {:.3f} V",
                    channelID, runtime.supplyVoltage, runtime.thresholdVoltage);
                nextStep();
            }
            else if(runtime.state == State::Discharging && runtime.thresholdVoltage >= runtime.loadVoltage)
            {
                Logger::logger().log_channel()->debug(
                    "Channel {} ending discharge step: load voltage {:.3f} V reached cutoff {:.3f} V",
                    channelID, runtime.loadVoltage, runtime.thresholdVoltage);
                nextStep();
            }
            else if(runtime.state == State::Holding && runtime.thresholdVoltage <= runtime.loadVoltage)
            {
                Logger::logger().log_channel()->debug(
                    "Channel {} ending hold step: load voltage {:.3f} V reached cutoff {:.3f} V",
                    channelID, runtime.loadVoltage, runtime.thresholdVoltage);
                nextStep();
            }
        }  
    }
}

void Channel::checkControlQueue()
{
    while (true)
    {
        // Calculate time elapsed
        std::string controlCmd;

        {
            std::lock_guard lock(controlMutex);

            if (controlQueue.empty())
                break;

            controlCmd = std::move(controlQueue.front());
            controlQueue.pop();
        }

        if (controlCmd == "start")
            startExperiment();
        else if (controlCmd == "stop")
            stopExperiment();
        else if (controlCmd == "pause")
            pauseExperiment();
        else if (controlCmd == "resume")
            resumeExperiment();
        else if (controlCmd == "skip")
            skipStep();
    }
}

void Channel::waitForControl(std::stop_token& stop)
{
    {
        std::unique_lock lock(controlMutex);
        controlCV.wait(lock, [this, &stop]()
        {
            return !controlQueue.empty() || stop.stop_requested();
        });
    }
}

void Channel::execute(const Charge& cmd)
{
    load.disable(loadChannel);
    runtime.loadEnabled = false;

    supply.setVoltage(cmd.voltage);
    supply.setCurrent(cmd.current);
    supply.enable();
    runtime.supplyEnabled = true;

    module.setToCharge();  
    runtime.moduleState = Module::State::CHARGE;
    
    runtime.state = State::Charging;
    runtime.thresholdVoltage = cmd.cutoffVoltage;
    runtime.totalDuration = cmd.duration;

    runtime.startTimePoint = std::chrono::steady_clock::now() - std::chrono::seconds(runtime.timeElapsed);
    Logger::logger().log_channel()->debug(
        "Channel {} starting charge: {:.3f} V, {:.3f} A, cutoff {:.3f} V, max {} s",
        channelID, cmd.voltage, cmd.current, cmd.cutoffVoltage, cmd.duration);
}

void Channel::execute(const Discharge& cmd)
{
    supply.disable();
    runtime.supplyEnabled = false;

    load.setCurrent(cmd.current, loadChannel);
    load.enable(loadChannel);
    runtime.loadEnabled = true;

    module.setToDischarge();
    runtime.moduleState = Module::State::DISCHARGE;

    runtime.state = State::Discharging;
    runtime.thresholdVoltage = cmd.cutoffVoltage;
    runtime.totalDuration = cmd.duration;

    runtime.startTimePoint = std::chrono::steady_clock::now() - std::chrono::seconds(runtime.timeElapsed);
    Logger::logger().log_channel()->debug(
        "Channel {} starting discharge: {:.3f} A, cutoff {:.3f} V, max {} s",
        channelID, cmd.current, cmd.cutoffVoltage, cmd.duration);
}

void Channel::execute(const Hold& cmd)
{
    load.setCurrent(0, loadChannel);
    load.disable(loadChannel);
    runtime.loadEnabled = false;

    supply.disable();
    runtime.supplyEnabled = false;

    module.setToDischarge(); 
    runtime.moduleState = Module::State::DISCHARGE;

    runtime.state = State::Holding;
    runtime.thresholdVoltage = cmd.cutoffVoltage;
    runtime.totalDuration = cmd.duration;

    runtime.startTimePoint = std::chrono::steady_clock::now() - std::chrono::seconds(runtime.timeElapsed);
    Logger::logger().log_channel()->debug(
        "Channel {} starting hold: cutoff {:.3f} V, max {} s",
        channelID, cmd.cutoffVoltage, cmd.duration);
}

void Channel::execute(const Rest& cmd)
{
    supply.disable();
    load.disable(loadChannel);
    runtime.supplyEnabled = false;
    runtime.loadEnabled = false;

    module.setToOff();
    runtime.moduleState = Module::State::OFF;

    runtime.state = State::Resting;
    runtime.totalDuration = cmd.duration;

    runtime.startTimePoint = std::chrono::steady_clock::now() - std::chrono::seconds(runtime.timeElapsed);
    Logger::logger().log_channel()->debug(
        "Channel {} starting rest: max {} s", channelID, cmd.duration);
}

void Channel::execute(const Goto& cmd)
{
    if (cmd.targetStep < 0 || static_cast<std::size_t>(cmd.targetStep) >= experiment.size() ||
        cmd.targetStep >= static_cast<int>(runtime.currentStep) || cmd.repeatCount < 0)
    {
        Logger::logger().log_channel()->error("Channel {} invalid goto: target {}, repeat count {}", channelID, cmd.targetStep, cmd.repeatCount);
        runtime.state = State::Error;
        return;
    }

   if (runtime.currentLoop >= cmd.repeatCount)
    {
        runtime.currentLoop = 0;
        Logger::logger().log_channel()->debug("Channel {} completing goto step after {} repeat(s)",channelID, cmd.repeatCount);
        nextStep();
        return;
    }else{
        ++runtime.currentLoop;
        runtime.currentStep = static_cast<std::size_t>(cmd.targetStep);

        Logger::logger().log_channel()->debug(
            "Channel {} repeating from step {} ({}/{})",
            channelID, runtime.currentStep, runtime.currentLoop, cmd.repeatCount);
    }

    return;
} 

void Channel::polling()
{
    load.restoreState();
    supply.restoreState();

    if(module.queryState() != runtime.moduleState)
    {
        if(runtime.moduleState == Module::CHARGE)
            module.setToCharge();
        else if(runtime.moduleState == Module::DISCHARGE)
            module.setToDischarge();
        else if(runtime.moduleState == Module::OFF)
            module.setToOff();
    }

    Load::Measurements loadMeasurements = load.measureAll(loadChannel);
    runtime.loadVoltage = loadMeasurements.voltage;
    runtime.loadCurrent = loadMeasurements.current;

    Supply::Measurements supplyMeasurements = supply.measureAll();
    runtime.supplyVoltage = supplyMeasurements.voltage;
    runtime.supplyCurrent = supplyMeasurements.current;

    Logger::logger().log_channel()->debug(
        "Channel {} measurements: load={:.3f} V, {:.3f} A; supply={:.3f} V, {:.3f} A",
        channelID, runtime.loadVoltage, runtime.loadCurrent, runtime.supplyVoltage, runtime.supplyCurrent);
}

bool Channel::startExperiment()
{
    if (runtime.state != State::Idle)
    {
        return false;
    }

    if (!ready)
    {
        Logger::logger().log_channel()->error("Channel {} is not ready yet",channelID);
        return false;
    }


    inProcess = true;
    running = true;
    return true;
}

bool Channel::pauseExperiment()
{
    if (runtime.state == State::Idle ||
        runtime.state == State::Paused ||
        runtime.state == State::Finished)
    {
        return false;
    }

    supply.disable();
    load.disable(loadChannel);
    module.setToOff();

    runtime.state = State::Paused;
    runtime.supplyEnabled = false;
    runtime.loadEnabled = false;
    runtime.moduleState = Module::OFF;

    running = false;

    runtime.timeElapsed = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - runtime.startTimePoint).count();
    return true;
}

bool Channel::resumeExperiment()
{
    if (runtime.state != State::Paused)
    {
        return false;
    }

    running = true;
    runtime.state = State::Idle;

    return true;
}

bool Channel::stopExperiment()
{
    if(runtime.state == State::Finished)
    {
        return false;
    }

    finishExperiment();
    return true;
}

bool Channel::skipStep()
{
    if(runtime.state == State::Paused || runtime.state == State::Idle || runtime.state == State::Finished)
    {
        return false;
    }
    nextStep();
    return true;
}

void Channel::finishExperiment()
{
    supply.disable();
    load.disable(loadChannel);
    module.setToOff();

    running = false;

    runtime = {};
    inProcess = false;
    runtime.state = State::Finished;
}

void Channel::nextStep()
{
    ++runtime.currentStep;
    runtime.timeElapsed = 0;

    if (runtime.currentStep >= experiment.size())
    {
        Logger::logger().log_channel()->debug(
            "Channel {} ending experiment: all steps completed", channelID);
        finishExperiment();
        return;
    }

    runtime.state = State::Idle;
}

std::string Channel::stateToString(State state)
{
    switch (state)
    {
        case State::Idle:         return "Idle";
        case State::Charging:     return "Charging";
        case State::Discharging:  return "Discharging";
        case State::Holding:      return "Holding";
        case State::Resting:      return "Resting";
        case State::Paused:       return "Paused";
        case State::Finished:     return "Finished";
        case State::Error:        return "Error";
        default:                  return "Unknown";
    }
}