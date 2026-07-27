#pragma once
#include "IDelay.h"
namespace jalari::hal { class Esp32Delay final : public IDelay { public: void milliseconds(unsigned long duration) const override; }; }
