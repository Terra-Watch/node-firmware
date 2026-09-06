#include "DtnStoreForwardService.h"

#include "ForwardingService.h"
#include "IClock.h"

namespace jalari::services
{
    DtnStoreForwardService::DtnStoreForwardService(
        const forwarding::IForwardingStrategy &strategy, ForwardingService &forwardingService,
        hal::IClock &clock, event::EventBus &eventBus, std::uint32_t queueLifetimeMs)
        : strategy_(strategy), forwardingService_(forwardingService), clock_(clock), eventBus_(eventBus),
          queueLifetimeMs_(queueLifetimeMs) {}

    bool DtnStoreForwardService::enqueue(const protocol::Packet &packet, DtnPriority priority)
    {
        if (packet.type != protocol::PacketType::Data || packet.destinationId == 0U ||
            packet.destinationId == 0xFFU || packet.ttl == 0U || contains_(packet))
        {
            publish_(event::Type::DtnPacketDropped, packet);
            return false;
        }

        const std::uint32_t nowMs = clock_.millis();
        const std::size_t freeIndex = selectEviction_(priority, nowMs);
        if (freeIndex >= entries_.size())
        {
            publish_(event::Type::DtnQueueFull, packet);
            publish_(event::Type::DtnPacketDropped, packet);
            return false;
        }

        if (entries_[freeIndex].active)
        {
            if ((nowMs - entries_[freeIndex].enqueuedMs) >= queueLifetimeMs_)
                publish_(event::Type::DtnPacketExpired, entries_[freeIndex].packet);
            else
                publish_(event::Type::DtnPacketDropped, entries_[freeIndex].packet);
        }
        entries_[freeIndex].packet = packet;
        entries_[freeIndex].priority = priority;
        entries_[freeIndex].enqueuedMs = nowMs;
        entries_[freeIndex].active = true;
        entries_[freeIndex].packet.nextHopId = 0U; // next hop is never retained in DTN state
        publish_(event::Type::DtnPacketQueued, packet);
        return true;
    }

    void DtnStoreForwardService::update()
    {
        const std::uint32_t nowMs = clock_.millis();
        for (Entry &entry : entries_)
        {
            if (entry.active && (nowMs - entry.enqueuedMs) >= queueLifetimeMs_)
            {
                publish_(event::Type::DtnPacketExpired, entry.packet);
                entry = {};
            }
        }

        std::size_t processed = 0U;
        const std::size_t initialCount = size();
        while (processed < initialCount && processed < constants::kDtnProcessingBatchSize)
        {
            std::size_t selected = entries_.size();
            for (std::size_t index = 0; index < entries_.size(); ++index)
            {
                if (!entries_[index].active) continue;
                if (selected >= entries_.size() || entries_[index].priority < entries_[selected].priority ||
                    (entries_[index].priority == entries_[selected].priority &&
                     entries_[index].enqueuedMs < entries_[selected].enqueuedMs))
                    selected = index;
            }
            if (selected >= entries_.size()) break;

            Entry candidate = entries_[selected];
            std::uint8_t nextHop = 0U;
            const forwarding::ForwardDecision decision = strategy_.decide(candidate.packet, nextHop);
            ++processed;
            if (decision == forwarding::ForwardDecision::RouteNotFound)
                continue; // retain it without spinning; it will be retried next tick
            if (decision != forwarding::ForwardDecision::Forward)
            {
                entries_[selected] = {};
                publish_(event::Type::DtnPacketDropped, candidate.packet);
                continue;
            }

            entries_[selected] = {};
            publish_(event::Type::DtnPacketDequeued, candidate.packet);
            candidate.packet.nextHopId = 0U;
            (void)forwardingService_.processReceived(candidate.packet);
            publish_(event::Type::DtnPacketForwarded, candidate.packet);
        }
    }

    std::size_t DtnStoreForwardService::size() const
    {
        std::size_t count = 0U;
        for (const Entry &entry : entries_) if (entry.active) ++count;
        return count;
    }

    bool DtnStoreForwardService::empty() const { return size() == 0U; }

    bool DtnStoreForwardService::contains_(const protocol::Packet &packet) const
    {
        for (const Entry &entry : entries_)
            if (entry.active && entry.packet.sourceId == packet.sourceId &&
                entry.packet.sequenceNumber == packet.sequenceNumber)
                return true;
        return false;
    }

    std::size_t DtnStoreForwardService::selectEviction_(DtnPriority incoming, std::uint32_t nowMs) const
    {
        for (std::size_t index = 0; index < entries_.size(); ++index)
            if (!entries_[index].active || (nowMs - entries_[index].enqueuedMs) >= queueLifetimeMs_)
                return index;

        std::size_t victim = entries_.size();
        for (std::size_t index = 0; index < entries_.size(); ++index)
        {
            if (entries_[index].priority < incoming) continue;
            if (victim >= entries_.size() || entries_[index].priority > entries_[victim].priority ||
                (entries_[index].priority == entries_[victim].priority &&
                 entries_[index].enqueuedMs < entries_[victim].enqueuedMs))
                victim = index;
        }
        return victim;
    }

    void DtnStoreForwardService::publish_(event::Type type, const protocol::Packet &packet) const
    {
        eventBus_.publish({type, packet});
    }
} // namespace jalari::services
