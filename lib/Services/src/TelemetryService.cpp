#include "TelemetryService.h"

#include <cstdio>

namespace jalari::services
{
    namespace
    {
        void put16(std::uint8_t *out, std::uint16_t value) { out[0] = value & 0xFFU; out[1] = value >> 8U; }
        constexpr std::uint8_t kTimeValid = 0x01U;
        constexpr std::uint8_t kTemperatureValid = 0x02U;
    }

    bool TelemetryService::begin()
    {
        initialized_ = rtc_.begin(sensorConfig_.i2cSdaPin, sensorConfig_.i2cSclPin);
        LOG_INFO(logger_, initialized_ ? "[SENSOR READY] sensor=DS3231 address=0x68" : "[SENSOR ERROR] sensor=DS3231 reason=INIT_FAILED");
        return initialized_;
    }

    void TelemetryService::update()
    {
        if (!initialized_) return;
        const std::uint32_t now = clock_.millis();
        if ((now - lastReadMs_) < config_.telemetryIntervalMs) return;
        lastReadMs_ = now;

        sensors::RtcSample sample;
        if (!rtc_.read(sample))
        {
            LOG_WARN(logger_, "[SENSOR ERROR] sensor=DS3231 reason=I2C_READ_FAILED");
            return;
        }

        char log[192];
        std::snprintf(log, sizeof(log), "[TELEMETRY SAMPLE] time_valid=%s temperature_valid=%s timestamp=%04u-%02u-%02uT%02u:%02u:%02u temperature_c=%.2f",
                      sample.timeValid ? "true" : "false", sample.temperatureValid ? "true" : "false",
                      sample.year, sample.month, sample.day, sample.hour, sample.minute, sample.second,
                      static_cast<double>(sample.temperatureCentiC) / 100.0);
        LOG_INFO(logger_, log);

        // DATA payload v1, little-endian: magic(1), version(1), validity(1),
        // year(2), month(1), day(1), hour(1), minute(1), second(1), temp_centiC(2).
        std::uint8_t payload[12] = {0x54U, 1U, 0U, 0U, 0U, 0U, 0U, 0U, 0U, 0U, 0U, 0U};
        if (sample.timeValid) payload[2] |= kTimeValid;
        if (sample.temperatureValid) payload[2] |= kTemperatureValid;
        put16(&payload[3], sample.year);
        payload[5] = sample.month; payload[6] = sample.day; payload[7] = sample.hour;
        payload[8] = sample.minute; payload[9] = sample.second;
        put16(&payload[10], static_cast<std::uint16_t>(sample.temperatureCentiC));

        protocol::Packet packet;
        if (!packetFactory_.createData(constants::kGatewayNodeId, payload, sizeof(payload), packet) || !packetQueue_.enqueue(packet))
        {
            LOG_WARN(logger_, "[TELEMETRY TX FAILED] reason=QUEUE_FULL_OR_PACKET_BUILD");
            return;
        }
        std::snprintf(log, sizeof(log), "[TELEMETRY TX] source=0x%02X destination=0x%02X sequence=%u ttl=%u hops=%u payloadSize=%u",
                      packet.sourceId, packet.destinationId, packet.sequenceNumber, packet.ttl, packet.hopCount,
                      static_cast<unsigned>(packet.payloadSize));
        LOG_INFO(logger_, log);
    }
} // namespace jalari::services
