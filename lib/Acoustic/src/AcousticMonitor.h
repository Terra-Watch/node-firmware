#pragma once
// Sound module: the MAX4466 energy detector wakes a capture of the INMP441 audio around the trigger.
// Two FreeRTOS tasks on core 0 (capture, trigger). There is no classifier yet: an event is logged with
// statistics of the 1 s window, and counted.

#include <cstdint>

namespace terrawatch::acoustic
{
    class AcousticMonitor
    {
    public:
        bool begin();
        std::uint32_t eventCount() const;
    };
} // namespace terrawatch::acoustic
