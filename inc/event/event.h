#pragma once

#include <optional>
#include <queue>
#include <mutex>
#include <condition_variable>

enum ChannelEventType
{
    LOAD_DISCONNECTED,
    SUPPLY_DISCONNECTED,
    MODULE_DISCONNECTED
};

struct ChannelEvent
{
    std::string channelID;
    std::string deviceIDN;
    std::string devicePort;
    ChannelEventType type;
    std::string message;
};

class ChannelEventBus
{
public:
    void push(ChannelEvent event);
    std::optional<ChannelEvent> waitForChannelEvent();

private:
    std::queue<ChannelEvent> queue;
    std::mutex mutex;
    std::condition_variable cv;
};