#pragma once

#include <cstdint>
#include "Packet.h"
#include "IClock.h"
#include "Logger.h"
#include "PacketQueue.h"

namespace jalari::services
{
    struct HeartbeatStats
    {
        std::uint32_t sent = 0;
        std::uint32_t ackReceived = 0;
        std::uint32_t retries = 0;
        std::uint32_t timeouts = 0;

        float successRatePct() const
        {
            return sent > 0 ? (static_cast<float>(ackReceived) / static_cast<float>(sent)) * 100.0f : 0.0f;
        }
    };

    class HeartbeatAckManager
    {
    public:
        enum class State
        {
            Idle,
            WaitingAck,
        };

        HeartbeatAckManager(hal::IClock &clock, utils::Logger &logger, queue::PacketQueue &packetQueue);

        void onHeartbeatTransmitted(std::uint32_t packetId, const protocol::Packet &packet);
        void onAckReceived(std::uint32_t ackPacketId, std::uint8_t nodeId, std::uint32_t rxTimestamp);
        void update();

        const HeartbeatStats &stats() const { return stats_; }

    private:
        hal::IClock &clock_;
        utils::Logger &logger_;
        queue::PacketQueue &packetQueue_;

        State state_ = State::Idle;
        std::uint32_t activePacketId_ = 0;
        std::uint32_t txTimestampMs_ = 0;
        std::uint8_t retryCount_ = 0;
        protocol::Packet activePacket_{};

        HeartbeatStats stats_{};
        static constexpr std::uint32_t kAckTimeoutMs = 1000U;
        static constexpr std::uint8_t kMaxRetries = 3U;
    };
} // namespace jalari::services
