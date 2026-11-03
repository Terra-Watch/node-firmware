#pragma once

#include "Config.h"

namespace terrawatch::hal
{
    class ISpiBus { public: virtual ~ISpiBus() = default; virtual void begin(const config::RadioConfig &config) = 0; };
} // namespace terrawatch::hal
