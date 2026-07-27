#pragma once

#include <cstdint>

#include "Config.h"
#include "EventBus.h"
#include "PacketFactory.h"
#include "PacketQueue.h"

namespace jalari::hal { class IClock; }

namespace jalari::services
{
    class HeartbeatService final : public event::IEventListener
    {
    public:
        HeartbeatService(const config::PowerConfig &config, protocol::PacketFactory &packetFactory,
                         queue::PacketQueue &packetQueue, hal::IClock &clock);
        bool begin(event::EventBus &eventBus);
        void update();
        void onEvent(const event::Event &event) override;

    private:
        const config::PowerConfig &config_;
        protocol::PacketFactory &packetFactory_;
        queue::PacketQueue &packetQueue_;
        hal::IClock &clock_;
        event::EventBus *eventBus_ = nullptr;
        std::uint32_t lastHeartbeatMs_ = 0;
    };
} // namespace jalari::services
