#include "NodeManager.h"

#include "Version.h"

namespace jalari::node
{

    NodeManager::NodeManager(const config::NodeConfig &config, DeviceRole role)
        : config_(config), role_(role)
    {
    }

    std::uint8_t NodeManager::id() const { return config_.nodeId; }
    DeviceRole NodeManager::role() const { return role_; }
    NodeStatus NodeManager::status() const { return status_; }
    const char *NodeManager::firmwareVersion() const { return version::kFirmwareVersion; }
    void NodeManager::setStatus(NodeStatus status) { status_ = status; }

} // namespace jalari::node
