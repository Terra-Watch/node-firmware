#pragma once

#include "IClock.h"

namespace terrawatch::hal
{
    class Esp32Clock final : public IClock { public: std::uint32_t millis() const override; };
} // namespace terrawatch::hal
