#pragma once
#include "ISpiBus.h"
namespace jalari::hal
{
    class Esp32SpiBus final : public ISpiBus { public: void begin(const config::RadioConfig &config) override; };
} // namespace jalari::hal
