#pragma once

#include "IConsole.h"

namespace jalari::utils
{

    enum class LogLevel : unsigned char
    {
        Trace,
        Debug,
        Info,
        Warn,
        Error,
        Off,
    };

#ifndef JALARI_LOG_LEVEL
#define JALARI_LOG_LEVEL 2
#endif

    class Logger
    {
    public:
        explicit Logger(hal::IConsole &console);
        void begin(unsigned long baudRate = 115200U);
        void trace(const char *message) const;
        void debug(const char *message) const;
        void info(const char *message) const;
        void warn(const char *message) const;
        void error(const char *message) const;

    private:
        void log_(LogLevel level, const char *tag, const char *message) const;
        hal::IConsole &console_;
    };

} // namespace jalari::utils

#define LOG_TRACE(logger, message) (logger).trace(message)
#define LOG_DEBUG(logger, message) (logger).debug(message)
#define LOG_INFO(logger, message) (logger).info(message)
#define LOG_WARN(logger, message) (logger).warn(message)
#define LOG_ERROR(logger, message) (logger).error(message)
