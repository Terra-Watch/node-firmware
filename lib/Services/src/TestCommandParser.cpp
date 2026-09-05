#include "TestCommandParser.h"

#include <cctype>
#include <cstdlib>
#include <cstring>

namespace jalari::services
{
    namespace
    {
        const char *skipSpaces(const char *text) { while (*text != '\0' && std::isspace(static_cast<unsigned char>(*text))) ++text; return text; }
        bool token(const char *&text, char *out, std::size_t capacity)
        {
            text = skipSpaces(text);
            if (*text == '\0') return false;
            std::size_t length = 0U;
            while (*text != '\0' && !std::isspace(static_cast<unsigned char>(*text)))
            {
                if (length + 1U >= capacity) return false;
                out[length++] = *text++;
            }
            out[length] = '\0';
            return true;
        }
        bool number(const char *text, unsigned long maximum, unsigned long &value)
        {
            if (text == nullptr || *text == '\0' || *text == '-') return false;
            char *end = nullptr;
            value = std::strtoul(text, &end, 0);
            return end != text && *end == '\0' && value <= maximum;
        }
    }

    bool parseTestCommand(const char *line, TestCommand &outCommand)
    {
        outCommand = {};
        if (line == nullptr || std::strlen(line) >= constants::kTestCommandLineCapacity) return false;
        const char *cursor = line;
        char command[20]{};
        char destination[16]{};
        char count[16]{};
        if (!token(cursor, command, sizeof(command)) || !token(cursor, destination, sizeof(destination))) return false;
        unsigned long destinationValue = 0U;
        if (!number(destination, 0xFEU, destinationValue) || destinationValue == 0U || destinationValue == 0xFFU) return false;

        if (std::strcmp(command, "SEND_DATA") == 0)
        {
            cursor = skipSpaces(cursor);
            if (*cursor == '\0') return false;
            const std::size_t payloadSize = std::strlen(cursor);
            if (payloadSize == 0U || payloadSize > constants::kMaxPayloadSize) return false;
            outCommand.type = TestCommandType::SendData;
            outCommand.count = 1U;
            outCommand.destinationId = static_cast<std::uint8_t>(destinationValue);
            outCommand.payloadSize = payloadSize;
            std::memcpy(outCommand.payload, cursor, payloadSize);
            return true;
        }

        if (std::strcmp(command, "SEND_DATA_COUNT") != 0 || !token(cursor, count, sizeof(count))) return false;
        unsigned long countValue = 0U;
        if (!number(count, constants::kTestDataCountMaximum, countValue) || countValue == 0U) return false;
        cursor = skipSpaces(cursor);
        if (*cursor == '\0') return false;
        const std::size_t payloadSize = std::strlen(cursor);
        if (payloadSize == 0U || payloadSize > constants::kMaxPayloadSize) return false;
        outCommand.type = TestCommandType::SendDataCount;
        outCommand.count = static_cast<std::uint16_t>(countValue);
        outCommand.destinationId = static_cast<std::uint8_t>(destinationValue);
        outCommand.payloadSize = payloadSize;
        std::memcpy(outCommand.payload, cursor, payloadSize);
        return true;
    }
} // namespace jalari::services
