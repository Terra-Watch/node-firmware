#pragma once

#include <array>
#include <cstddef>

#include "Constants.h"
#include "Packet.h"

namespace jalari::queue
{
    class PacketQueue
    {
    public:
        bool enqueue(const protocol::Packet &packet);
        bool dequeue(protocol::Packet &outPacket);
        const protocol::Packet *peek() const;
        bool empty() const;
        bool full() const;
        std::size_t size() const;
        void clear();

    private:
        std::array<protocol::Packet, constants::kPacketQueueCapacity> packets_{};
        std::size_t head_ = 0;
        std::size_t tail_ = 0;
        std::size_t size_ = 0;
    };
} // namespace jalari::queue
