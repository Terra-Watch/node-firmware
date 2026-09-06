#include <cassert>
#include <cstring>

#include "TestCommandParser.h"

int main() {
    using namespace jalari::services;
    TestCommand command;
    assert(parseTestCommand("SEND_DATA 2 HELLO", command));
    assert(command.type == TestCommandType::SendData && command.destinationId == 2U && command.count == 1U);
    assert(command.payloadSize == 5U && std::memcmp(command.payload, "HELLO", 5U) == 0);
    assert(!parseTestCommand("SEND_DATA hello TEST", command));
    assert(!parseTestCommand("SEND_DATA 0 TEST", command));
    assert(!parseTestCommand("SEND_DATA 255 TEST", command));
    assert(!parseTestCommand("SEND_DATA 2", command));
    char oversized[64] = "SEND_DATA 2 ";
    for (std::size_t index = 12U; index < 63U; ++index) oversized[index] = 'X';
    oversized[63] = '\0';
    assert(!parseTestCommand(oversized, command));
    assert(parseTestCommand("SEND_DATA_COUNT 2 100 TEST", command));
    assert(command.type == TestCommandType::SendDataCount && command.count == 100U);
    assert(!parseTestCommand("SEND_DATA_COUNT 2 101 TEST", command));
    assert(!parseTestCommand("SEND_DATA_COUNT 2 0 TEST", command));
    assert(!parseTestCommand("SEND_DATA 2", command));
    return 0;
}
