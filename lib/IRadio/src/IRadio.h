#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "Constants.h"

namespace terrawatch::radio
{
    struct ReceivedFrame
    {
        std::array<std::uint8_t, constants::kMaxRadioBufferSize> bytes{};
        std::size_t length = 0;
        int rssi = 0;
        float snr = 0.0F;
    };

    class IRadio
    {
    public:
        virtual ~IRadio() = default;
        virtual bool begin() = 0;
        virtual bool send(const std::uint8_t *data, std::size_t length) = 0;
        virtual bool receive(ReceivedFrame &outFrame) = 0;
        virtual bool isInitialized() const = 0;
        virtual int rssi() const = 0;
        virtual float snr() const = 0;
        virtual bool cad() = 0;
        // Listen before talk: true while the modem sees a LoRa signal (preamble, sync or a packet in progress). Radios that cannot tell say false.
        virtual bool channelBusy() { return false; }
    };
} // namespace terrawatch::radio
