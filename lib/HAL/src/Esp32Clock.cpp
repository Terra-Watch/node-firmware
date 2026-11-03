#include "Esp32Clock.h"
#include <Arduino.h>
namespace terrawatch::hal { std::uint32_t Esp32Clock::millis() const { return ::millis(); } }
