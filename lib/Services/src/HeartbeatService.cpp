#include "HeartbeatService.h"

#include "IClock.h"

namespace jalari::services
{
    HeartbeatService::HeartbeatService(const config::PowerConfig &config, protocol::PacketFactory &packetFactory,
                                       queue::PacketQueue &packetQueue, hal::IClock &clock)
        : config_(config), packetFactory_(packetFactory), packetQueue_(packetQueue), clock_(clock) {}

    bool HeartbeatService::begin(event::EventBus &eventBus)
    {
        eventBus_ = &eventBus;
        return eventBus.subscribe(*this);
    }

    void HeartbeatService::update()
    {
        const std::uint32_t now = clock_.millis();
        if ((now - lastHeartbeatMs_) < config_.heartbeatIntervalMs) return;
        protocol::Packet packet;
        if (packetFactory_.createHeartbeat(packet) && packetQueue_.enqueue(packet)) lastHeartbeatMs_ = now;
    }

    void HeartbeatService::onEvent(const event::Event &event)
    {
        if (event.type == event::Type::PacketSent && event.packet.type == protocol::PacketType::Heartbeat && eventBus_ != nullptr)
        {
            eventBus_->publish({event::Type::HeartbeatSent, event.packet});
        }
    }
} // namespace jalari::services
