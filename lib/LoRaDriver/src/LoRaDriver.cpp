#include "LoRaDriver.h"
#include <Arduino.h>
#include <SPI.h>
#define private public   // the library keeps readRegister() private; listen-before-talk and the diagnostic build read the modem status directly
#include <LoRa.h>
#undef private

namespace terrawatch::lora
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
#ifdef TW_TX_POWER_DBM
            LoRa.setTxPower(TW_TX_POWER_DBM); // 2..17 dBm; lowering it shrinks the range, which lets a bench test force a packet through the relay
#endif
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
#ifdef TW_RF_DIAGNOSTIC
        lastTxEndMs_ = millis();
#endif

        LoRa.enableCrc();
        LoRa.receive();
        return success;
    }

    bool LoRaDriver::receive(radio::ReceivedFrame &outFrame) {
        outFrame = {};
        if (!initialized_) return false;
#ifdef TW_RF_DIAGNOSTIC
        // Modem interrupt flags before parsePacket() clears them: 0x10 valid header, 0x40 receive done, 0x20 payload CRC error.
        static uint8_t lastFlags = 0;
        const uint8_t flags = LoRa.readRegister(0x12);
        if ((flags & 0x70) && flags != lastFlags) Serial.printf("[RF DIAG] modem flags 0x%02X (valid header %d, rx done %d, payload CRC error %d), bytes %d\n", flags, !!(flags & 0x10), !!(flags & 0x40), !!(flags & 0x20), LoRa.readRegister(0x13));
        lastFlags = flags;
#endif
        const int packetSize = LoRa.parsePacket();
#ifdef TW_RF_DIAGNOSTIC
        // Channel energy while nothing decodes: shows whether the other radio's transmissions reach this antenna at all.
        static int maxRssi = -200; static unsigned long lastReport = 0;
        const int now = LoRa.rssi(); if (now > maxRssi) maxRssi = now;
        if (now > -85) Serial.printf("[RF DIAG] channel RSSI %d dBm, %lu ms after our last transmission ended\n", now, millis() - lastTxEndMs_);
        if (millis() - lastReport > 2000UL) { Serial.printf("[RF DIAG] max channel RSSI last 2 s: %d dBm\n", maxRssi); maxRssi = -200; lastReport = millis(); }
        if (packetSize > 0) Serial.printf("[RF DIAG] packet decoded: %d bytes rssi %d snr %.1f\n", packetSize, LoRa.packetRssi(), LoRa.packetSnr());
#endif
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

    bool LoRaDriver::channelBusy()
    {
        // RegModemStat (0x18) in continuous receive: bit0 signal detected, bit1 signal synchronized, bit2 RX ongoing, bit3 header valid.
        // Bits 0 and 2 are set by noise and by plain listening (measured: they made every send look busy), so only a locked-on signal counts.
        constexpr std::uint8_t kModemStat = 0x18U, kBusyMask = 0x0AU;
        return initialized_ && (LoRa.readRegister(kModemStat) & kBusyMask) != 0U;
    }
} // namespace terrawatch::lora
