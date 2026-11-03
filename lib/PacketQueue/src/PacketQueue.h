#pragma once

#include <array>
#include <cstddef>

#include "Constants.h"
#include "Packet.h"

#if defined(ARDUINO)
#include <freertos/FreeRTOS.h>
#include <freertos/portmacro.h>
#endif

namespace terrawatch::queue
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
        void lock_() const;
        void unlock_() const;
        std::array<protocol::Packet, constants::kPacketQueueCapacity> packets_{};
        std::size_t head_ = 0;
        std::size_t tail_ = 0;
        std::size_t size_ = 0;
#if defined(ARDUINO)
        mutable portMUX_TYPE mutex_ = portMUX_INITIALIZER_UNLOCKED;
#endif
    };
} // namespace terrawatch::queue
