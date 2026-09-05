#pragma once

#include <cstddef>
#include <cstdint>

#include "Constants.h"

namespace jalari::services
{
    enum class TestCommandType : std::uint8_t { Invalid, SendData, SendDataCount };

    struct TestCommand
    {
        TestCommandType type = TestCommandType::Invalid;
        std::uint8_t destinationId = 0U;
        std::uint16_t count = 0U;
        std::size_t payloadSize = 0U;
        std::uint8_t payload[constants::kMaxPayloadSize]{};
    };

    bool parseTestCommand(const char *line, TestCommand &outCommand);
} // namespace jalari::services
