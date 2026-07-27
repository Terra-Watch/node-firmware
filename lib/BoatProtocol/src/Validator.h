#pragma once

#include "Packet.h"

namespace jalari::protocol
{

    class Validator
    {
    public:
        static bool isValid(const Packet &packet);
    };

} // namespace jalari::protocol
