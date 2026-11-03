#pragma once
#include "ISpiBus.h"
namespace terrawatch::hal
{
    class Esp32SpiBus final : public ISpiBus { public: void begin(const config::RadioConfig &config) override; };
} // namespace terrawatch::hal
