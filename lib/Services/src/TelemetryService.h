#pragma once

#include <cstdint>

#include "Config.h"
#include "II2cBus.h"
#include "Logger.h"
#include "PacketFactory.h"
#include "PacketQueue.h"
#include "Ds3231.h"
#include "IClock.h"

namespace jalari::services
{
    class TelemetryService
    {
    public:
        TelemetryService(const config::PowerConfig &config, const config::SensorConfig &sensorConfig,
                         sensors::Ds3231 &rtc, protocol::PacketFactory &packetFactory,
                         queue::PacketQueue &packetQueue, hal::IClock &clock, utils::Logger &logger)
            : config_(config), sensorConfig_(sensorConfig), rtc_(rtc), packetFactory_(packetFactory),
              packetQueue_(packetQueue), clock_(clock), logger_(logger) {}

        bool begin();
        void update();

    private:
        const config::PowerConfig &config_;
        const config::SensorConfig &sensorConfig_;
        sensors::Ds3231 &rtc_;
        protocol::PacketFactory &packetFactory_;
        queue::PacketQueue &packetQueue_;
        hal::IClock &clock_;
        utils::Logger &logger_;
        std::uint32_t lastReadMs_ = 0;
        bool initialized_ = false;
    };
} // namespace jalari::services
