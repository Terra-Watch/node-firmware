#pragma once

#include "Config.h"
#include "EventBus.h"
#include "NeighborManager.h"

namespace jalari::hal { class IClock; }

namespace jalari::services
{
    class NeighborService final : public event::IEventListener
    {
    public:
        NeighborService(const config::NetworkConfig &config, neighbor::NeighborManager &neighborManager,
                        hal::IClock &clock);
        bool begin(event::EventBus &eventBus);
        void update();
        void onEvent(const event::Event &event) override;

    private:
        void publishMetricsUpdated_(std::uint8_t nodeId);
        const config::NetworkConfig &config_;
        neighbor::NeighborManager &neighborManager_;
        hal::IClock &clock_;
        event::EventBus *eventBus_ = nullptr;
    };
} // namespace jalari::services
