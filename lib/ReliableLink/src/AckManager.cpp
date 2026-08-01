#include "AckManager.h"

#include "NodeManager.h"

namespace jalari::reliable
{
    bool DuplicatePacketCache::isDuplicateAndRemember(std::uint8_t sourceId, protocol::PacketType type, std::uint16_t sequenceNumber)
    {
        for (const Entry &entry : entries_)
        {
            if (entry.active && entry.sourceId == sourceId && entry.type == type && entry.sequenceNumber == sequenceNumber) return true;
        }
        Entry &entry = entries_[nextEntry_];
        entry.sourceId = sourceId;
        entry.type = type;
        entry.sequenceNumber = sequenceNumber;
        entry.active = true;
        nextEntry_ = (nextEntry_ + 1U) % entries_.size();
        return false;
    }

    AckManager::AckManager(const config::ReliabilityConfig &config, const node::NodeManager &node,
                           protocol::PacketFactory &packetFactory, queue::PacketQueue &packetQueue,
                           hal::IClock &clock)
        : config_(config), node_(node), packetFactory_(packetFactory), packetQueue_(packetQueue), clock_(clock) {}

    bool AckManager::begin(event::EventBus &eventBus)
    {
        eventBus_ = &eventBus;
        return eventBus.subscribe(*this);
    }

    void AckManager::update()
    {
        const std::uint32_t now = clock_.millis();
        for (PendingTransmission &pending : pending_)
        {
            if (!pending.active || pending.retryQueued ||
                (now - pending.lastTransmittedMs) < config_.acknowledgementTimeoutMs) continue;
            if (pending.retries >= config_.maxRetries)
            {
                if (eventBus_ != nullptr)
                {
                    eventBus_->publish({event::Type::AcknowledgementFailed, pending.packet});
                    eventBus_->publish({event::Type::PacketDropped, pending.packet});
                }
                pending.active = false;
                continue;
            }
            if (packetQueue_.enqueue(pending.packet)) { ++pending.retries; pending.retryQueued = true; }
        }
    }

    void AckManager::onEvent(const event::Event &event)
    {
        if (event.type == event::Type::PacketReceived) handleReceived_(event.packet);
        if (event.type == event::Type::PacketSent) handleTransmitted_(event.packet);
    }

    bool AckManager::isDuplicateAndRemember(std::uint8_t sourceId, protocol::PacketType type, std::uint16_t sequenceNumber)
    {
        return duplicates_.isDuplicateAndRemember(sourceId, type, sequenceNumber);
    }

    std::size_t AckManager::pendingCount() const
    {
        std::size_t pendingCount = 0;
        for (const PendingTransmission &pending : pending_) if (pending.active) ++pendingCount;
        return pendingCount;
    }

    void AckManager::handleReceived_(const protocol::Packet &packet)
    {
        if (packet.destinationId != node_.id()) return;
        if (packet.type == protocol::PacketType::Acknowledgement) { acknowledge_(packet); return; }
        if (packet.type != protocol::PacketType::Data) return;
        (void)duplicates_.isDuplicateAndRemember(packet.sourceId, packet.type, packet.sequenceNumber);
        protocol::Packet acknowledgement;
        if (packetFactory_.createAcknowledgement(packet, acknowledgement)) (void)packetQueue_.enqueue(acknowledgement);
    }

    void AckManager::handleTransmitted_(const protocol::Packet &packet)
    {
        if (packet.type != protocol::PacketType::Data) return;
        PendingTransmission *pending = findPending_(packet.sequenceNumber);
        if (pending == nullptr)
        {
            pending = allocatePending_();
            if (pending == nullptr) return;
            pending->packet = packet;
            pending->retries = 0;
            pending->active = true;
        }
        pending->lastTransmittedMs = clock_.millis();
        pending->retryQueued = false;
    }

    void AckManager::acknowledge_(const protocol::Packet &packet)
    {
        const std::uint16_t acknowledgedSequence = static_cast<std::uint16_t>(packet.payload[0]) |
                                                   (static_cast<std::uint16_t>(packet.payload[1]) << 8U);
        PendingTransmission *pending = findPending_(acknowledgedSequence);
        if (pending != nullptr && pending->packet.destinationId == packet.sourceId)
        {
            pending->active = false;
            if (eventBus_ != nullptr) eventBus_->publish({event::Type::AcknowledgementReceived, packet});
        }
    }

    AckManager::PendingTransmission *AckManager::findPending_(std::uint16_t sequenceNumber)
    {
        for (PendingTransmission &pending : pending_)
        {
            if (pending.active && pending.packet.sequenceNumber == sequenceNumber) return &pending;
        }
        return nullptr;
    }

    AckManager::PendingTransmission *AckManager::allocatePending_()
    {
        for (PendingTransmission &pending : pending_) if (!pending.active) return &pending;
        return nullptr;
    }
} // namespace jalari::reliable
