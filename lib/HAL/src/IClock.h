#pragma once

#include <cstdint>

namespace terrawatch::hal
{
    class IClock { public: virtual ~IClock() = default; virtual std::uint32_t millis() const = 0; };
} // namespace terrawatch::hal
