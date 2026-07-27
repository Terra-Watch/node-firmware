#include "ReliableLinkService.h"

#include "AckManager.h"

namespace jalari::services
{
    ReliableLinkService::ReliableLinkService(reliable::AckManager &ackManager) : ackManager_(ackManager) {}
    bool ReliableLinkService::begin(event::EventBus &eventBus) { return ackManager_.begin(eventBus); }
    void ReliableLinkService::update() { ackManager_.update(); }
} // namespace jalari::services
