#include "ReliableLinkService.h"

#include "AckManager.h"

namespace terrawatch::services
{
    ReliableLinkService::ReliableLinkService(reliable::AckManager &ackManager) : ackManager_(ackManager) {}
    bool ReliableLinkService::begin(event::EventBus &eventBus) { return ackManager_.begin(eventBus); }
    void ReliableLinkService::update() { ackManager_.update(); }
    bool ReliableLinkService::processReceivedData(const protocol::Packet &packet) { return ackManager_.processReceivedData(packet); }
    void ReliableLinkService::onTransmissionFailed(const protocol::Packet &packet) { ackManager_.onTransmissionFailed(packet); }
    bool ReliableLinkService::readyForTestPacket() const { return ackManager_.pendingCount() == 0U; }
    bool ReliableLinkService::isDuplicate(std::uint8_t sourceId, std::uint16_t sequenceNumber) { return ackManager_.isDuplicateAndRemember(sourceId, sequenceNumber); }
} // namespace terrawatch::services
