#pragma once

#include "Config.h"
#include "IRadio.h"
#include "ISpiBus.h"

namespace jalari::lora
{
    class LoRaDriver final : public radio::IRadio
    {
    public:
        LoRaDriver(const config::RadioConfig &config, hal::ISpiBus &spiBus);
        bool begin() override;
        bool send(const std::uint8_t *data, std::size_t length) override;
        bool receive(radio::ReceivedFrame &outFrame) override;
        bool isInitialized() const override;
        int rssi() const override;
        float snr() const override;
        bool cad() override;
    private:
        const config::RadioConfig &config_;
        hal::ISpiBus &spiBus_;
        bool initialized_ = false;
        int lastRssi_ = 0;
        float lastSnr_ = 0.0F;
    };
} // namespace jalari::lora
