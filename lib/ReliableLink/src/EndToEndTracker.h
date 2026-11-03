#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "Packet.h"

namespace terrawatch::reliable
{
    // The hop-by-hop ACK only says the next node took the packet. This keeps a copy of what a field node sent to the gateway until the gateway's
    // own receipt (PacketType::EndToEndAck) comes back, and offers it again when it does not. Pure logic: the caller passes the time.
    class EndToEndTracker
    {
    public:
        static constexpr std::size_t kCapacity = 4;
        static constexpr std::uint8_t kMaxRetries = 3;
        // Wait after the latest send before the next retry. The first is longer than the 10 s duplicate memory of the relays, so they forward it again.
        static constexpr std::uint32_t kRetryAfterMs[kMaxRetries] = {12000U, 20000U, 30000U};

        bool track(const protocol::Packet &packet, std::uint32_t nowMs); // false when the table is full or the packet is already tracked
        bool confirm(std::uint16_t sequenceNumber, std::uint8_t &retriesUsed);
        bool nextRetry(std::uint32_t nowMs, protocol::Packet &outPacket);
        std::uint32_t confirmedCount() const { return confirmed_; }
        std::uint32_t gaveUpCount() const { return gaveUp_; }
        std::size_t pendingCount() const;

    private:
        struct Entry { protocol::Packet packet{}; std::uint32_t lastSentMs = 0; std::uint8_t retries = 0; bool active = false; };
        std::array<Entry, kCapacity> entries_{};
        std::uint32_t confirmed_ = 0;
        std::uint32_t gaveUp_ = 0;
    };
} // namespace terrawatch::reliable
