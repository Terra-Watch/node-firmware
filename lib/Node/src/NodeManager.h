#pragma once

#include <cstdint>

#include "Config.h"

namespace jalari::node
{

    enum class DeviceRole : std::uint8_t
    {
        Unknown = 0,
        BoatNode = 1,
        ShoreGateway = 2,
    };

    enum class NodeStatus : std::uint8_t
    {
        Booting = 0,
        Ready = 1,
        Fault = 2,
    };

    class NodeManager
    {
    public:
        NodeManager(const config::NodeConfig &config, DeviceRole role);

        std::uint8_t id() const;
        DeviceRole role() const;
        NodeStatus status() const;
        const char *firmwareVersion() const;
        void setStatus(NodeStatus status);

    private:
        const config::NodeConfig &config_;
        const DeviceRole role_;
        NodeStatus status_ = NodeStatus::Booting;
    };

} // namespace jalari::node
