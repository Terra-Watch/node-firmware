#pragma once
// Sound module: the MAX4466 energy detector wakes a capture of the INMP441 audio around the trigger.
// Two FreeRTOS tasks on core 0 (capture, trigger). There is no classifier yet: an event is logged with
// statistics of the 1 s window, and counted.

#include <cstdint>

#include "AlertLogic.h"

namespace terrawatch::acoustic
{
    class AcousticMonitor
    {
    public:
        bool begin();
        std::uint32_t eventCount() const;
        // Called (on the classifier task) for every alert the classifier raises: kind, probability in percent. Set before begin(). Only used when TW_ACOUSTIC_MODEL=1.
        using AlertSink = void (*)(const Alert &alert);
        static void setAlertSink(AlertSink sink);
    };
} // namespace terrawatch::acoustic
