#pragma once
#include "IConsole.h"
namespace jalari::hal
{
    class Esp32SerialConsole final : public IConsole { public: void begin(unsigned long baudRate) override; void print(const char *text) override; void println(const char *text) override; };
} // namespace jalari::hal
