#include "HeartbeatAckManager.h"
#include <cstdio>

namespace jalari::services
{
    HeartbeatAckManager::HeartbeatAckManager(hal::IClock &clock, utils::Logger &logger, queue::PacketQueue &packetQueue,
                                             const node::NodeManager &node)
        : clock_(clock), logger_(logger), packetQueue_(packetQueue), node_(node) {}

    void HeartbeatAckManager::onHeartbeatTransmitted(std::uint32_t packetId, const protocol::Packet &packet)
    {
        activePacketId_ = packetId;
        activePacket_ = packet;
        txTimestampMs_ = clock_.millis();
        retryCount_ = 0;
        state_ = State::WaitingAck;

        stats_.sent++;

        char msg[128];
        snprintf(msg, sizeof(msg), "[Heartbeat #%lu]", static_cast<unsigned long>(activePacketId_));
        LOG_INFO(logger_, msg);
        LOG_INFO(logger_, "Sent");
        LOG_INFO(logger_, "Waiting ACK...");
    }

    void HeartbeatAckManager::onAckReceived(const protocol::Packet &packet)
    {
        if (state_ != State::WaitingAck || packet.type != protocol::PacketType::HeartbeatAck ||
            packet.payloadSize != 9U || packet.sourceId != constants::kGatewayNodeId ||
            packet.destinationId != node_.id() || packet.payload[4] != node_.id() ||
            packet.payload[4] != packet.destinationId)
        {
            LOG_WARN(logger_, "[HEARTBEAT ACK REJECTED] Invalid type/source/target/payload");
            return;
        }

        const std::uint32_t ackPacketId = static_cast<std::uint32_t>(packet.payload[0]) |
                                          (static_cast<std::uint32_t>(packet.payload[1]) << 8U) |
                                          (static_cast<std::uint32_t>(packet.payload[2]) << 16U) |
                                          (static_cast<std::uint32_t>(packet.payload[3]) << 24U);
        const std::uint32_t rxTimestamp = static_cast<std::uint32_t>(packet.payload[5]) |
                                          (static_cast<std::uint32_t>(packet.payload[6]) << 8U) |
                                          (static_cast<std::uint32_t>(packet.payload[7]) << 16U) |
                                          (static_cast<std::uint32_t>(packet.payload[8]) << 24U);
        if (ackPacketId != activePacketId_)
        {
            LOG_WARN(logger_, "[HEARTBEAT ACK REJECTED] Packet ID does not match active heartbeat");
            return;
        }

        const std::uint32_t rtt = clock_.millis() - txTimestampMs_;
        stats_.ackReceived++;
        state_ = State::Idle;

        char msg[128];
        snprintf(msg, sizeof(msg), "[HEARTBEAT ACK RX] type: %u | source: 0x%02X | target: 0x%02X | packet ID: %lu | gateway timestamp: %lu",
                 static_cast<unsigned>(packet.type), static_cast<unsigned>(packet.sourceId),
                 static_cast<unsigned>(packet.destinationId), static_cast<unsigned long>(ackPacketId),
                 static_cast<unsigned long>(rxTimestamp));
        LOG_INFO(logger_, msg);
        LOG_INFO(logger_, "[HEARTBEAT ACK VALID]");
        snprintf(msg, sizeof(msg), "RTT = %lu ms", static_cast<unsigned long>(rtt));
        LOG_INFO(logger_, msg);
        snprintf(msg, sizeof(msg), "Stats: Sent=%lu | ACK=%lu | Retries=%lu | Timeouts=%lu | Success=%.1f%%",
                 static_cast<unsigned long>(stats_.sent),
                 static_cast<unsigned long>(stats_.ackReceived),
                 static_cast<unsigned long>(stats_.retries),
                 static_cast<unsigned long>(stats_.timeouts),
                 stats_.successRatePct());
        LOG_INFO(logger_, msg);
    }

    void HeartbeatAckManager::update()
    {
        if (state_ != State::WaitingAck)
        {
            return;
        }

        const std::uint32_t now = clock_.millis();
        if ((now - txTimestampMs_) >= kAckTimeoutMs)
        {
            if (retryCount_ < kMaxRetries)
            {
                retryCount_++;
                stats_.retries++;
                txTimestampMs_ = now;

                char msg[64];
                snprintf(msg, sizeof(msg), "Retry %u", retryCount_);
                LOG_WARN(logger_, msg);

                (void)packetQueue_.enqueue(activePacket_);
            }
            else
            {
                stats_.timeouts++;
                state_ = State::Idle;

                LOG_ERROR(logger_, "FAILED (No ACK after 3 retries)");
                char msg[128];
                snprintf(msg, sizeof(msg), "Stats: Sent=%lu | ACK=%lu | Retries=%lu | Timeouts=%lu | Success=%.1f%%",
                         static_cast<unsigned long>(stats_.sent),
                         static_cast<unsigned long>(stats_.ackReceived),
                         static_cast<unsigned long>(stats_.retries),
                         static_cast<unsigned long>(stats_.timeouts),
                         stats_.successRatePct());
                LOG_INFO(logger_, msg);
            }
        }
    }
} // namespace jalari::services
