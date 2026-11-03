#pragma once

#include <cstdint>

#include "Bme680.h"
#include "Config.h"
#include "IClock.h"
#include "Logger.h"
#include "Pms5003.h"
#include "PacketFactory.h"
#include "PacketQueue.h"

namespace terrawatch::services
{
    class TelemetryService
    {
    public:
        TelemetryService(const config::PowerConfig &config, const config::SensorConfig &sensorConfig,
                         sensors::Bme680 &bme, sensors::Pms5003 &pms, protocol::PacketFactory &packetFactory,
                         queue::PacketQueue &packetQueue, hal::IClock &clock, utils::Logger &logger)
            : config_(config), sensorConfig_(sensorConfig), bme_(bme), pms_(pms), packetFactory_(packetFactory),
              packetQueue_(packetQueue), clock_(clock), logger_(logger) {}

        bool begin();
        void update();

    private:
        const config::PowerConfig &config_;
        const config::SensorConfig &sensorConfig_;
        sensors::Bme680 &bme_;
        sensors::Pms5003 &pms_;
        sensors::Pms5003Sample pmsLatest_{};
        bool pmsSeen_ = false;
        protocol::PacketFactory &packetFactory_;
        queue::PacketQueue &packetQueue_;
        hal::IClock &clock_;
        utils::Logger &logger_;
        std::uint32_t lastReadMs_ = 0;
        bool initialized_ = false;
    };
} // namespace terrawatch::services
