#pragma once
#include "IConsole.h"
namespace jalari::hal
{
    class Esp32SerialConsole final : public IConsole { public: void begin(unsigned long baudRate) override; void print(const char *text) override; void println(const char *text) override; bool readLine(char *buffer, std::size_t capacity) override; private: char line_[128]{}; std::size_t length_ = 0U; };
} // namespace jalari::hal
