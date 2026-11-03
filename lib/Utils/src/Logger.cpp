#include "Logger.h"

// Console speed. The NodeMCU-32S relay logs at 921600 so that a burst of log lines ends before the radio's reply arrives (slower logging corrupts reception on that board).
#ifndef TW_CONSOLE_BAUD
#define TW_CONSOLE_BAUD 115200U
#endif

namespace terrawatch::utils
{

    Logger::Logger(hal::IConsole &console) : console_(console)
    {
        console_.begin(TW_CONSOLE_BAUD);
    }
    void Logger::begin(unsigned long baudRate) {}
    void Logger::trace(const char *message) const { log_(LogLevel::Trace, "TRACE", message); }
    void Logger::debug(const char *message) const { log_(LogLevel::Debug, "DEBUG", message); }
    void Logger::info(const char *message) const { log_(LogLevel::Info, "INFO", message); }
    void Logger::warn(const char *message) const { log_(LogLevel::Warn, "WARN", message); }
    void Logger::error(const char *message) const { log_(LogLevel::Error, "ERROR", message); }

    void Logger::log_(LogLevel level, const char *tag, const char *message) const
    {
        if (static_cast<unsigned char>(level) < TW_LOG_LEVEL) return;
        console_.print("["); console_.print(tag); console_.print("] "); console_.println(message);
    }

} // namespace terrawatch::utils
