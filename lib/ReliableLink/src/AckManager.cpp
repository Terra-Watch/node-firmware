#include "AckManager.h"

#include "NodeManager.h"

namespace jalari::reliable
{
    bool DuplicatePacketCache::isDuplicateAndRemember(std::uint8_t sourceId, std::uint16_t sequenceNumber,
                                                      std::uint32_t nowMs, std::uint32_t expiryMs)
    {
        for (Entry &entry : entries_)
        {
            if (entry.active && (nowMs - entry.seenMs) >= expiryMs) entry.active = false;
            if (entry.active && entry.sourceId == sourceId && entry.sequenceNumber == sequenceNumber) return true;
        }
        Entry &entry = entries_[nextEntry_];
        entry.sourceId = sourceId;
        entry.sequenceNumber = sequenceNumber;
        entry.seenMs = nowMs;
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
            scheduleRetryOrFail_(pending);
        }
    }

    void AckManager::onEvent(const event::Event &event)
    {
        if (event.type == event::Type::PacketReceived) handleReceived_(event.packet);
        if (event.type == event::Type::PacketSent) handleTransmitted_(event.packet);
    }

    bool AckManager::processReceivedData(const protocol::Packet &packet)
    {
        if (packet.type != protocol::PacketType::Data) return false;

        const bool duplicate = duplicates_.isDuplicateAndRemember(packet.sourceId, packet.sequenceNumber,
                                                                   clock_.millis(), constants::kDuplicateExpiryMs);
        if (duplicate && eventBus_ != nullptr) eventBus_->publish({event::Type::DuplicateSuppressed, packet});

        if (packet.destinationId != 0xFFU && packet.previousHopId != 0U)
        {
            protocol::Packet acknowledgement;
            if (packetFactory_.createAcknowledgement(packet, acknowledgement) &&
                !packetQueue_.enqueue(acknowledgement) && eventBus_ != nullptr)
                eventBus_->publish({event::Type::PacketDropped, acknowledgement});
        }
        return !duplicate;
    }

    void AckManager::onTransmissionFailed(const protocol::Packet &packet)
    {
        if (packet.type != protocol::PacketType::Data) return;
        PendingTransmission *pending = findPending_(packet.sequenceNumber);
        if (pending == nullptr)
        {
            pending = allocatePending_();
            if (pending == nullptr)
            {
                if (eventBus_ != nullptr) eventBus_->publish({event::Type::TransmissionFailed, packet});
                return;
            }
            pending->packet = packet;
            pending->retries = 0;
            pending->active = true;
        }
        scheduleRetryOrFail_(*pending);
    }

    bool AckManager::isDuplicateAndRemember(std::uint8_t sourceId, std::uint16_t sequenceNumber)
    {
        return duplicates_.isDuplicateAndRemember(sourceId, sequenceNumber, clock_.millis(), constants::kDuplicateExpiryMs);
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
        // DATA is preprocessed by processReceivedData before PacketReceived is published.
    }

    void AckManager::scheduleRetryOrFail_(PendingTransmission &pending)
    {
        if (pending.retries >= config_.maxRetries)
        {
            if (eventBus_ != nullptr)
            {
                eventBus_->publish({event::Type::TransmissionFailed, pending.packet});
                eventBus_->publish({event::Type::AcknowledgementFailed, pending.packet});
                eventBus_->publish({event::Type::PacketDropped, pending.packet});
            }
            pending.active = false;
            return;
        }

        if (!packetQueue_.enqueue(pending.packet))
        {
            if (eventBus_ != nullptr)
            {
                eventBus_->publish({event::Type::TransmissionFailed, pending.packet});
                eventBus_->publish({event::Type::PacketDropped, pending.packet});
            }
            pending.active = false;
            return;
        }

        ++pending.retries;
        pending.retryQueued = true;
        if (eventBus_ != nullptr) eventBus_->publish({event::Type::TransmissionRetryQueued, pending.packet});
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
        const bool expectedNextHop = pending != nullptr &&
                                     (pending->packet.nextHopId != 0U
                                          ? pending->packet.nextHopId == packet.sourceId
                                          : pending->packet.destinationId == packet.sourceId);
        if (expectedNextHop)
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
