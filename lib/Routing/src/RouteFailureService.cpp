#include "RouteFailureService.h"

namespace jalari::routing
{
    bool RouteFailureService::begin(event::EventBus &eventBus)
    {
        eventBus_ = &eventBus;
        return eventBus.subscribe(*this);
    }

    void RouteFailureService::onEvent(const event::Event &event)
    {
        // This event is emitted only after the configured ACK retries are exhausted.
        if (event.type != event::Type::AcknowledgementFailed ||
            event.packet.type != protocol::PacketType::Data ||
            event.packet.destinationId == 0U || event.packet.destinationId == 0xFFU ||
            event.packet.nextHopId == 0U)
            return;

        if (routeTable_.invalidateRoute(event.packet.destinationId, event.packet.nextHopId) && eventBus_ != nullptr)
        {
            eventBus_->publish({event::Type::RouteInvalidated, event.packet});
            eventBus_->publish({event::Type::ForwardingRouteNotFound, event.packet});
        }
    }
} // namespace jalari::routing
