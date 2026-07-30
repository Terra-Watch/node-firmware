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

#ifdef SIMULATION_MODE
        constexpr std::uint32_t intervalMs = 5000U;
        constexpr std::uint8_t batteryPct = 95U;
        constexpr std::uint8_t status = 0U; // NORMAL
#else
        const std::uint32_t intervalMs = config_.heartbeatIntervalMs;
        const std::uint8_t batteryPct = 0xFFU; // placeholder when unavailable
        const std::uint8_t status = 0U; // NORMAL
#endif

        if ((now - lastHeartbeatMs_) < intervalMs) return;
        protocol::Packet packet;
        if (packetFactory_.createHeartbeat(now, batteryPct, status, packet) && packetQueue_.enqueue(packet))
        {
            lastHeartbeatMs_ = now;
        }
    }

    void HeartbeatService::onEvent(const event::Event &event)
    {
        if (event.type == event::Type::PacketSent && event.packet.type == protocol::PacketType::Heartbeat && eventBus_ != nullptr)
        {
            eventBus_->publish({event::Type::HeartbeatSent, event.packet});
        }
    }
} // namespace jalari::services
