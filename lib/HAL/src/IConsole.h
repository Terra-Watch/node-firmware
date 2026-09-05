#pragma once

#include <cstddef>

namespace jalari::hal
{
    class IConsole { public: virtual ~IConsole() = default; virtual void begin(unsigned long baudRate) = 0; virtual void print(const char *text) = 0; virtual void println(const char *text) = 0; virtual bool readLine(char *buffer, std::size_t capacity) = 0; };
} // namespace jalari::hal
