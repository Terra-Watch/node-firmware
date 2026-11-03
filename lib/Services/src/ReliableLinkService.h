#pragma once

#include "EndToEndTracker.h"
#include "Packet.h"

namespace terrawatch::event { class EventBus; }
namespace terrawatch::reliable { class AckManager; }

namespace terrawatch::services
{
    class ReliableLinkService
    {
    public:
        explicit ReliableLinkService(reliable::AckManager &ackManager);
        bool begin(event::EventBus &eventBus);
        void update();
        bool processReceivedData(const protocol::Packet &packet);
        void onTransmissionFailed(const protocol::Packet &packet);
        bool readyForTestPacket() const;
        // End to end: keep what this node sent to the gateway until the gateway confirms it, and offer it again if it does not.
        void trackEndToEnd(const protocol::Packet &sent, std::uint32_t nowMs) { (void)endToEnd_.track(sent, nowMs); }
        bool confirmEndToEnd(std::uint16_t sequenceNumber, std::uint8_t &retriesUsed) { return endToEnd_.confirm(sequenceNumber, retriesUsed); }
        bool nextEndToEndRetry(std::uint32_t nowMs, protocol::Packet &outPacket) { return endToEnd_.nextRetry(nowMs, outPacket); }
        bool isDuplicate(std::uint8_t sourceId, std::uint16_t sequenceNumber);
        const reliable::EndToEndTracker &endToEnd() const { return endToEnd_; }

    private:
        reliable::AckManager &ackManager_;
        reliable::EndToEndTracker endToEnd_;
    };
} // namespace terrawatch::services
