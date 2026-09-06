#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "Config.h"
#include "Constants.h"
#include "EventBus.h"
#include "IClock.h"
#include "PacketFactory.h"
#include "PacketQueue.h"

namespace jalari::node { class NodeManager; }

namespace jalari::reliable
{
    class DuplicatePacketCache
    {
    public:
        bool isDuplicateAndRemember(std::uint8_t sourceId, std::uint16_t sequenceNumber,
                                    std::uint32_t nowMs, std::uint32_t expiryMs);

    private:
        struct Entry { std::uint8_t sourceId = 0; std::uint16_t sequenceNumber = 0; std::uint32_t seenMs = 0; bool active = false; };
        std::array<Entry, constants::kDuplicatePacketCapacity> entries_{};
        std::size_t nextEntry_ = 0;
    };

    class AckManager final : public event::IEventListener
    {
    public:
        AckManager(const config::ReliabilityConfig &config, const node::NodeManager &node,
                   protocol::PacketFactory &packetFactory, queue::PacketQueue &packetQueue,
                   hal::IClock &clock);

        bool begin(event::EventBus &eventBus);
        void update();
        void onEvent(const event::Event &event) override;
        bool processReceivedData(const protocol::Packet &packet);
        void onTransmissionFailed(const protocol::Packet &packet);
        bool isDuplicateAndRemember(std::uint8_t sourceId, std::uint16_t sequenceNumber);
        std::size_t pendingCount() const;

    private:
        struct PendingTransmission
        {
            protocol::Packet packet{};
            std::uint32_t lastTransmittedMs = 0;
            std::uint8_t retries = 0;
            bool retryQueued = false;
            bool active = false;
        };

        void handleReceived_(const protocol::Packet &packet);
        void handleTransmitted_(const protocol::Packet &packet);
        void scheduleRetryOrFail_(PendingTransmission &pending);
        void acknowledge_(const protocol::Packet &packet);
        PendingTransmission *findPending_(std::uint16_t sequenceNumber);
        PendingTransmission *allocatePending_();

        const config::ReliabilityConfig &config_;
        const node::NodeManager &node_;
        protocol::PacketFactory &packetFactory_;
        queue::PacketQueue &packetQueue_;
        hal::IClock &clock_;
        event::EventBus *eventBus_ = nullptr;
        DuplicatePacketCache duplicates_;
        std::array<PendingTransmission, constants::kPendingTransmissionCapacity> pending_{};
    };
} // namespace jalari::reliable
