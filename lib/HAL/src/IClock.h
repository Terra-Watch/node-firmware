#pragma once

#include <cstdint>

namespace jalari::hal
{
    class IClock { public: virtual ~IClock() = default; virtual std::uint32_t millis() const = 0; };
} // namespace jalari::hal
