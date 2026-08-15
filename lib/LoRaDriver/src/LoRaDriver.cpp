#include "LoRaDriver.h"
#include <LoRa.h>

namespace jalari::lora
{
    LoRaDriver::LoRaDriver(const config::RadioConfig &config, hal::ISpiBus &spiBus) : config_(config), spiBus_(spiBus) {}
    bool LoRaDriver::begin()
    {
        pinMode(config_.chipSelectPin, OUTPUT);
        digitalWrite(config_.chipSelectPin, HIGH);
        pinMode(config_.resetPin, OUTPUT);
        digitalWrite(config_.resetPin, HIGH);

        spiBus_.begin(config_);
        LoRa.setPins(config_.chipSelectPin, config_.resetPin, config_.dio0Pin);

        initialized_ = LoRa.begin(config_.frequencyHz);
        if (initialized_)
        {
            LoRa.setSpreadingFactor(7);
            LoRa.setSignalBandwidth(125000L);
            LoRa.setCodingRate4(5);
            LoRa.setSyncWord(0x12);
            LoRa.enableCrc();
            LoRa.receive();
        }
        return initialized_;
    }

    bool LoRaDriver::send(const std::uint8_t *data, std::size_t length)
    {
        if (!initialized_ || data == nullptr || length == 0U || length > constants::kMaxRadioBufferSize) return false;

        LoRa.idle();
        LoRa.beginPacket();
        LoRa.enableCrc();
        const std::size_t written = LoRa.write(data, static_cast<int>(length));
        if (written != length)
        {
            LoRa.endPacket();
            LoRa.enableCrc();
            LoRa.receive();
            return false;
        }
        const bool success = (LoRa.endPacket() == 1);

        LoRa.enableCrc();
        LoRa.receive();
        return success;
    }

    bool LoRaDriver::receive(radio::ReceivedFrame &outFrame) {
        outFrame = {};
        if (!initialized_) return false;
        const int packetSize = LoRa.parsePacket();
        if (packetSize <= 0) return false;

        const auto boundedSize = static_cast<std::size_t>(packetSize) < outFrame.bytes.size() ? static_cast<std::size_t>(packetSize) : outFrame.bytes.size();
        for (std::size_t index = 0; index < boundedSize && LoRa.available(); ++index) {
            outFrame.bytes[index] = static_cast<std::uint8_t>(LoRa.read());
        }
        while (LoRa.available()) (void)LoRa.read();
        outFrame.length = boundedSize;

        outFrame.rssi = LoRa.packetRssi();
        outFrame.snr = LoRa.packetSnr();
        lastRssi_ = outFrame.rssi;
        lastSnr_ = outFrame.snr;
        return outFrame.length > 0U;
    }

    bool LoRaDriver::isInitialized() const { return initialized_; }
    int LoRaDriver::rssi() const { return lastRssi_; }
    float LoRaDriver::snr() const { return lastSnr_; }
    bool LoRaDriver::cad() { return false; }
} // namespace jalari::lora
