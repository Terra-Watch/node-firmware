#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "Constants.h"
#include "EventBus.h"
#include "ForwardingService.h"
#include "IForwardingStrategy.h"
#include "Packet.h"

namespace jalari::hal { class IClock; }
namespace jalari::services { class ForwardingService; }

namespace jalari::services
{
    enum class DtnPriority : std::uint8_t
    {
        Sos = 0,
        Normal = 1,
        Background = 2,
    };

    class DtnStoreForwardService final : public IDtnStoreForwardSink
    {
    public:
        DtnStoreForwardService(const forwarding::IForwardingStrategy &strategy,
                               ForwardingService &forwardingService, hal::IClock &clock,
                               event::EventBus &eventBus,
                               std::uint32_t queueLifetimeMs = constants::kDtnQueueLifetimeMs);

        bool enqueue(const protocol::Packet &packet, DtnPriority priority);
        bool enqueue(const protocol::Packet &packet) override { return enqueue(packet, DtnPriority::Normal); }
        void update();
        std::size_t size() const;
        bool empty() const;

    private:
        struct Entry
        {
            protocol::Packet packet{};
            DtnPriority priority = DtnPriority::Normal;
            std::uint32_t enqueuedMs = 0U;
            bool active = false;
        };

        bool contains_(const protocol::Packet &packet) const;
        std::size_t selectEviction_(DtnPriority incoming, std::uint32_t nowMs) const;
        void publish_(event::Type type, const protocol::Packet &packet) const;

        const forwarding::IForwardingStrategy &strategy_;
        ForwardingService &forwardingService_;
        hal::IClock &clock_;
        event::EventBus &eventBus_;
        const std::uint32_t queueLifetimeMs_;
        std::array<Entry, constants::kDtnQueueCapacity> entries_{};
    };
} // namespace jalari::services
