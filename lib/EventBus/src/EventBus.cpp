#include "EventBus.h"
namespace jalari::event
{
    bool EventBus::subscribe(IEventListener &listener) { for (auto *registered : listeners_) if (registered == &listener) return true; for (auto &registered : listeners_) if (registered == nullptr) { registered = &listener; return true; } return false; }
    bool EventBus::unsubscribe(IEventListener &listener) { for (auto &registered : listeners_) if (registered == &listener) { registered = nullptr; return true; } return false; }
    void EventBus::publish(const Event &event) const { for (auto *listener : listeners_) if (listener != nullptr) listener->onEvent(event); }
} // namespace jalari::event
