#pragma once

#include "Packet.h"

namespace terrawatch::protocol
{

    class Validator
    {
    public:
        static bool isValid(const Packet &packet);
    };

} // namespace terrawatch::protocol
