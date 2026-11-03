#pragma once
namespace terrawatch::hal { class IDelay { public: virtual ~IDelay() = default; virtual void milliseconds(unsigned long duration) const = 0; }; }
