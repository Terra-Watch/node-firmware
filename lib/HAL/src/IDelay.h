#pragma once
namespace jalari::hal { class IDelay { public: virtual ~IDelay() = default; virtual void milliseconds(unsigned long duration) const = 0; }; }
