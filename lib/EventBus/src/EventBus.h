#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "Constants.h"
#include "Packet.h"

namespace jalari::event
{
    enum class Type : unsigned char
    {
        PacketReceived,
        PacketSent,
        PacketDropped,
        TtlExpired,
        HeartbeatSent,
        HeartbeatReceived,
        NeighborExpired,
        LinkMetricsUpdated,
        AcknowledgementReceived,
        AcknowledgementFailed,
        TransmissionRetryQueued,
        TransmissionFailed,
        DuplicateSuppressed,
        ForwardingRouteNotFound,
        RouteAdvertisementReceived,
        RouteAdvertisementSuppressed,
        RouteAdvertisementRejected,
        RouteInvalidated,
        RouteRecovered,
        DtnPacketQueued,
        DtnPacketDequeued,
        DtnPacketExpired,
        DtnPacketDropped,
        DtnQueueFull,
        DtnPacketForwarded,
        RadioReady,
        RadioError,
    };
    struct Event
    {
        Event(Type eventType) : type(eventType) {}
        Event(Type eventType, const protocol::Packet &eventPacket, int eventRssi = 0, float eventSnr = 0.0F)
            : type(eventType), packet(eventPacket), rssi(eventRssi), snr(eventSnr) {}
        Type type;
        protocol::Packet packet{};
        int rssi = 0;
        float snr = 0.0F;
        std::uint8_t nodeId = 0;
    };
    class IEventListener { public: virtual ~IEventListener() = default; virtual void onEvent(const Event &event) = 0; };
    class EventBus
    {
    public:
        bool subscribe(IEventListener &listener);
        bool unsubscribe(IEventListener &listener);
        void publish(const Event &event) const;
    private:
        std::array<IEventListener *, constants::kEventBusMaxListeners> listeners_{};
    };
} // namespace jalari::event
