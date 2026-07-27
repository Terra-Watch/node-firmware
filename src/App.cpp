#include "App.h"

#include <array>

#include "ForwardingService.h"
#include "HeartbeatService.h"
#include "IRadio.h"
#include "Logger.h"
#include "NeighborService.h"
#include "NodeManager.h"
#include "PacketQueue.h"
#include "Parser.h"
#include "ReliableLinkService.h"
#include "Serializer.h"
#include "Validator.h"
#include "Version.h"

namespace jalari
{
    App::App(radio::IRadio &radio, node::NodeManager &node, queue::PacketQueue &packetQueue, event::EventBus &eventBus,
             services::HeartbeatService &heartbeatService, services::NeighborService &neighborService,
             services::ReliableLinkService &reliableLinkService, services::ForwardingService &forwardingService,
             utils::Logger &logger)
        : radio_(radio), node_(node), packetQueue_(packetQueue), eventBus_(eventBus), heartbeatService_(heartbeatService),
          neighborService_(neighborService), reliableLinkService_(reliableLinkService),
          forwardingService_(forwardingService), logger_(logger) {}

    bool App::begin()
    {
        logger_.begin();
        LOG_INFO(logger_, "================================");
        LOG_INFO(logger_, version::kFirmwareName);
        LOG_INFO(logger_, version::kFirmwareVersion);
        LOG_INFO(logger_, "================================");
        if (!eventBus_.subscribe(*this) || !heartbeatService_.begin(eventBus_) ||
            !neighborService_.begin(eventBus_) || !reliableLinkService_.begin(eventBus_))
        {
            LOG_ERROR(logger_, "Event bus subscription failed");
            return false;
        }
        if (!radio_.begin())
        {
            node_.setStatus(node::NodeStatus::Fault);
            eventBus_.publish({event::Type::RadioError});
            return false;
        }
        node_.setStatus(node::NodeStatus::Ready);
        initialized_ = true;
        eventBus_.publish({event::Type::RadioReady});
        return true;
    }

    void App::update()
    {
        if (!initialized_) return;
        processIncoming_();
        reliableLinkService_.update();
        heartbeatService_.update();
        neighborService_.update();
        transmitQueued_();
    }

    void App::transmitQueued_()
    {
        const protocol::Packet *packet = packetQueue_.peek();
        if (packet == nullptr) return;
        std::array<std::uint8_t, constants::kMaxPacketSize> buffer{};
        std::size_t bufferSize = 0;
        if (!protocol::Validator::isValid(*packet) || !protocol::Serializer::serialize(*packet, buffer, bufferSize) || !radio_.send(buffer.data(), bufferSize))
        {
            LOG_WARN(logger_, "Packet transmission failed");
            return;
        }
        protocol::Packet sentPacket;
        (void)packetQueue_.dequeue(sentPacket);
        eventBus_.publish({event::Type::PacketSent, sentPacket});
    }

    void App::processIncoming_()
    {
        radio::ReceivedFrame frame;
        if (!radio_.receive(frame)) return;
        protocol::Packet packet;
        if (!protocol::Parser::parse(frame.bytes.data(), frame.length, packet) || !protocol::Validator::isValid(packet))
        {
            LOG_WARN(logger_, "Received invalid packet");
            return;
        }
        if (forwardingService_.processReceived(packet)) eventBus_.publish({event::Type::PacketReceived, packet, frame.rssi, frame.snr});
    }

    void App::onEvent(const event::Event &event)
    {
        switch (event.type)
        {
        case event::Type::HeartbeatSent: LOG_INFO(logger_, "Heartbeat packet sent"); break;
        case event::Type::RadioReady: LOG_INFO(logger_, "LoRa ready"); break;
        case event::Type::RadioError: LOG_ERROR(logger_, "LoRa initialization failed"); break;
        default: break;
        }
    }
} // namespace jalari
