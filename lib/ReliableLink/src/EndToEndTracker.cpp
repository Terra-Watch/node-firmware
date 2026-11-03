#include "EndToEndTracker.h"

namespace terrawatch::reliable
{
    constexpr std::uint32_t EndToEndTracker::kRetryAfterMs[];

    bool EndToEndTracker::track(const protocol::Packet &packet, std::uint32_t nowMs)
    {
        Entry *free = nullptr;
        for (Entry &entry : entries_)
        {
            if (entry.active && entry.packet.sequenceNumber == packet.sequenceNumber) return false;
            if (!entry.active && free == nullptr) free = &entry;
        }
        if (free == nullptr) return false;
        free->packet = packet; free->lastSentMs = nowMs; free->retries = 0; free->active = true;
        return true;
    }

    bool EndToEndTracker::confirm(std::uint16_t sequenceNumber, std::uint8_t &retriesUsed)
    {
        for (Entry &entry : entries_)
            if (entry.active && entry.packet.sequenceNumber == sequenceNumber)
            {
                entry.active = false; ++confirmed_; retriesUsed = entry.retries;
                return true;
            }
        return false;
    }

    bool EndToEndTracker::nextRetry(std::uint32_t nowMs, protocol::Packet &outPacket)
    {
        for (Entry &entry : entries_)
        {
            if (!entry.active || (nowMs - entry.lastSentMs) < kRetryAfterMs[entry.retries < kMaxRetries ? entry.retries : kMaxRetries - 1U]) continue;
            if (entry.retries >= kMaxRetries) { entry.active = false; ++gaveUp_; continue; }
            ++entry.retries; entry.lastSentMs = nowMs;
            outPacket = entry.packet;
            outPacket.nextHopId = 0U;
            return true;
        }
        return false;
    }

    std::size_t EndToEndTracker::pendingCount() const
    {
        std::size_t count = 0;
        for (const Entry &entry : entries_) if (entry.active) ++count;
        return count;
    }
} // namespace terrawatch::reliable
