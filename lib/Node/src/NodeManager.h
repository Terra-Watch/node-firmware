#pragma once

#include <cstdint>

#include "Config.h"

namespace terrawatch::node
{

    enum class DeviceRole : std::uint8_t
    {
        Unknown = 0,
        FieldNode = 1,
        Gateway = 2,
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
        config::NodeConfig config_;      // by value: a reference dangled when a caller passed a temporary (found by AddressSanitizer in next_hop and dynamic_forwarding tests)
        const DeviceRole role_;
        NodeStatus status_ = NodeStatus::Booting;
    };

} // namespace terrawatch::node
