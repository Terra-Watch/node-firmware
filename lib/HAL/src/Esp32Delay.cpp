#include "Esp32Delay.h"
#include <Arduino.h>
namespace terrawatch::hal { void Esp32Delay::milliseconds(unsigned long duration) const { ::delay(duration); } }
