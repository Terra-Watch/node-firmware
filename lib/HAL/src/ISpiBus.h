#pragma once

#include "Config.h"

namespace jalari::hal
{
    class ISpiBus { public: virtual ~ISpiBus() = default; virtual void begin(const config::RadioConfig &config) = 0; };
} // namespace jalari::hal
