#pragma once

#include "EventBus.h"
#include "RouteTable.h"

namespace jalari::routing
{
    class RouteFailureService final : public event::IEventListener
    {
    public:
        explicit RouteFailureService(RouteTable &routeTable) : routeTable_(routeTable) {}
        bool begin(event::EventBus &eventBus);
        void onEvent(const event::Event &event) override;

    private:
        RouteTable &routeTable_;
        event::EventBus *eventBus_ = nullptr;
    };
} // namespace jalari::routing
