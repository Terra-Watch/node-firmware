#include "TelemetryService.h"

#include <cstdio>

namespace terrawatch::services
{
    namespace
    {
        void put16(std::uint8_t *out, std::uint16_t value) { out[0] = value & 0xFFU; out[1] = value >> 8U; }
        void put32(std::uint8_t *out, std::uint32_t value) { put16(out, value & 0xFFFFU); put16(out + 2, value >> 16U); }
        constexpr std::uint8_t kBmeValid = 0x01U;
        constexpr std::uint8_t kPmsValid = 0x02U;
        constexpr std::uint16_t kNotPresent = 0xFFFFU;
    }

    bool TelemetryService::begin()
    {
        pms_.begin(sensorConfig_.pmsRxPin, sensorConfig_.pmsTxPin, sensorConfig_.pmsSetPin);
        initialized_ = bme_.begin(sensorConfig_.i2cSdaPin, sensorConfig_.i2cSclPin);
        LOG_INFO(logger_, initialized_ ? "[SENSOR READY] sensor=BME680" : "[SENSOR ERROR] sensor=BME680 reason=INIT_FAILED (check wiring and address)");
        return initialized_;
    }

    void TelemetryService::update()
    {
        pmsSeen_ |= pms_.read(pmsLatest_); // keep draining the UART between telemetry intervals
#if defined(TW_STATION_LAT_E6) && defined(TW_STATION_LON_E6)
        // Position payload v1, little-endian, 10 bytes: 'P'(1), version(1), latitude(4) and longitude(4) as int32 micro-degrees. The station has no GPS: this is
        // the fixed place it was installed at (build flags), sent once at start and every kPositionIntervalMs after, so the dashboard can check its registry entry.
        constexpr std::uint32_t kPositionIntervalMs = 600000U;
        const std::uint32_t t = clock_.millis();
        if (!positionSent_ ? t > 20000U : (t - lastPositionMs_) >= kPositionIntervalMs)
        {
            std::uint8_t pos[10] = {0x50U, 1U};
            put32(&pos[2], static_cast<std::uint32_t>(static_cast<std::int32_t>(TW_STATION_LAT_E6)));
            put32(&pos[6], static_cast<std::uint32_t>(static_cast<std::int32_t>(TW_STATION_LON_E6)));
            protocol::Packet packet;
            if (packetFactory_.createData(constants::kGatewayNodeId, pos, sizeof(pos), packet) && packetQueue_.enqueue(packet))
            {
                positionSent_ = true; lastPositionMs_ = t;
                LOG_INFO(logger_, "[POSITION TX] fixed station position queued");
            }
        }
#endif
        if (!initialized_) return;
        const std::uint32_t now = clock_.millis();
        if ((now - lastReadMs_) < config_.telemetryIntervalMs) return;
        lastReadMs_ = now;

        sensors::Bme680Sample sample;
        if (!bme_.read(sample))
        {
            LOG_WARN(logger_, "[SENSOR ERROR] sensor=BME680 reason=READ_FAILED");
            return;
        }

        // DATA payload v1, little-endian, 15 bytes: 'S'(1), version(1), flags(1), pm25(2), pm10(2), pm25_baseline(2),
        // temp_centiC(2), humidity_centi%(2), battery_mV(2). Fields without a sensor yet are 0xFFFF.
        std::uint8_t payload[15] = {0x53U, 1U, static_cast<std::uint8_t>(kBmeValid | (pmsSeen_ ? kPmsValid : 0U))};
        put16(&payload[3], pmsSeen_ ? pmsLatest_.pm25 : kNotPresent);
        put16(&payload[5], pmsSeen_ ? pmsLatest_.pm10 : kNotPresent);
        put16(&payload[7], kNotPresent);
        put16(&payload[9], static_cast<std::uint16_t>(sample.temperatureCentiC));
        put16(&payload[11], sample.humidityCentiPercent);
        put16(&payload[13], kNotPresent);

        char log[160];
        std::snprintf(log, sizeof(log), "[TELEMETRY SAMPLE] temperature_c=%.2f humidity_pct=%.2f pm25=%d pm10=%d pms_bytes=%lu",
                      static_cast<double>(sample.temperatureCentiC) / 100.0, static_cast<double>(sample.humidityCentiPercent) / 100.0,
                      pmsSeen_ ? pmsLatest_.pm25 : -1, pmsSeen_ ? pmsLatest_.pm10 : -1, static_cast<unsigned long>(pms_.bytesSeen()));
        LOG_INFO(logger_, log);

        protocol::Packet packet;
        if (!packetFactory_.createData(constants::kGatewayNodeId, payload, sizeof(payload), packet) || !packetQueue_.enqueue(packet))
        {
            LOG_WARN(logger_, "[TELEMETRY TX FAILED] reason=QUEUE_FULL_OR_PACKET_BUILD");
            return;
        }
        std::snprintf(log, sizeof(log), "[TELEMETRY TX] source=0x%02X destination=0x%02X sequence=%u ttl=%u payloadSize=%u",
                      packet.sourceId, packet.destinationId, packet.sequenceNumber, packet.ttl,
                      static_cast<unsigned>(packet.payloadSize));
        LOG_INFO(logger_, log);
    }
} // namespace terrawatch::services
