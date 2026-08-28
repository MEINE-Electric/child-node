#include "event/event.h"

void ChannelEventBus::push(ChannelEvent event)
{
    {
        std::lock_guard lock(mutex);
        queue.push(std::move(event));
    }
    
    cv.notify_one();
}

std::optional<ChannelEvent> 
ChannelEventBus::waitForChannelEvent()
{
    std::unique_lock lock(mutex);
    
    if(!cv.wait_for(lock, std::chrono::milliseconds(100),[this] {
        return !queue.empty();
    }))
    {
        return std::nullopt;
    }

    auto event = std::move(queue.front());
    queue.pop();

    return event;
}