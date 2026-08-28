#pragma once

#include <optional>
#include <queue>
#include <mutex>
#include <condition_variable>

enum ChannelEventType
{
    LOAD_DISCONNECTED = 101,
    SUPPLY_DISCONNECTED = 201,
    MODULE_DISCONNECTED = 301,
};

struct ChannelEvent
{
    std::string channelID;
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