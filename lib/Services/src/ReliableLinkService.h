#pragma once

namespace jalari::event { class EventBus; }
namespace jalari::reliable { class AckManager; }

namespace jalari::services
{
    class ReliableLinkService
    {
    public:
        explicit ReliableLinkService(reliable::AckManager &ackManager);
        bool begin(event::EventBus &eventBus);
        void update();

    private:
        reliable::AckManager &ackManager_;
    };
} // namespace jalari::services
