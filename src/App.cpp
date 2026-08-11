#include "App.h"
#include <Arduino.h>

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

#include "HeartbeatAckManager.h"

namespace jalari
{
    App::App(radio::IRadio &radio, node::NodeManager &node, queue::PacketQueue &packetQueue, event::EventBus &eventBus,
             services::HeartbeatService &heartbeatService, services::HeartbeatAckManager &heartbeatAckManager,
             services::NeighborService &neighborService, services::ReliableLinkService &reliableLinkService,
             services::ForwardingService &forwardingService, utils::Logger &logger)
        : radio_(radio), node_(node), packetQueue_(packetQueue), eventBus_(eventBus), heartbeatService_(heartbeatService),
          heartbeatAckManager_(heartbeatAckManager), neighborService_(neighborService),
          reliableLinkService_(reliableLinkService), forwardingService_(forwardingService), logger_(logger) {}

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
        heartbeatAckManager_.update();
        neighborService_.update();
        transmitQueued_();
    }

    void App::transmitQueued_()
    {
        const protocol::Packet *packet = packetQueue_.peek();
        if (packet == nullptr) return;

        bool isHeartbeat = (packet->type == protocol::PacketType::Heartbeat);
        std::array<std::uint8_t, constants::kMaxPacketSize> buffer{};
        std::size_t bufferSize = 0;

        bool serialized = protocol::Validator::isValid(*packet) &&
                          protocol::Serializer::serialize(*packet, buffer, bufferSize);
        bool success = serialized && radio_.send(buffer.data(), bufferSize);

        if (isHeartbeat)
        {
            LOG_INFO(logger_, "--------------------------------");
            LOG_INFO(logger_, "[Heartbeat TX]");

            char msg[64];
            snprintf(msg, sizeof(msg), "Node ID: %u", packet->sourceId);
            LOG_INFO(logger_, msg);

            snprintf(msg, sizeof(msg), "Sequence: %u", packet->sequenceNumber);
            LOG_INFO(logger_, msg);

            snprintf(msg, sizeof(msg), "Packet Size: %u", static_cast<unsigned int>(bufferSize));
            LOG_INFO(logger_, msg);

            std::uint32_t uptimeMs = 0;
            if (packet->payloadSize >= 4)
            {
                uptimeMs = static_cast<std::uint32_t>(packet->payload[0]) |
                           (static_cast<std::uint32_t>(packet->payload[1]) << 8U) |
                           (static_cast<std::uint32_t>(packet->payload[2]) << 16U) |
                           (static_cast<std::uint32_t>(packet->payload[3]) << 24U);
            }
            snprintf(msg, sizeof(msg), "Timestamp: %u ms", uptimeMs);
            LOG_INFO(logger_, msg);

            snprintf(msg, sizeof(msg), "Transmission Result: %s", success ? "success" : "failure");
            LOG_INFO(logger_, msg);

            LOG_INFO(logger_, "--------------------------------");
        }

        if (!success)
        {
            LOG_WARN(logger_, "Packet transmission failed");
            protocol::Packet failedPacket;
            (void)packetQueue_.dequeue(failedPacket);
            return;
        }

        protocol::Packet sentPacket;
        (void)packetQueue_.dequeue(sentPacket);
        if (sentPacket.type == protocol::PacketType::Heartbeat && sentPacket.payloadSize >= 4)
        {
            std::uint32_t pktId = static_cast<std::uint32_t>(sentPacket.payload[0]) |
                                  (static_cast<std::uint32_t>(sentPacket.payload[1]) << 8U) |
                                  (static_cast<std::uint32_t>(sentPacket.payload[2]) << 16U) |
                                  (static_cast<std::uint32_t>(sentPacket.payload[3]) << 24U);
            heartbeatAckManager_.onHeartbeatTransmitted(pktId, sentPacket);
        }
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

        if (packet.sourceId == node_.id())
        {
            return; // Ignore self-echo from radio
        }

        if (packet.type == protocol::PacketType::HeartbeatAck && packet.payloadSize >= 9)
        {
            std::uint32_t ackPktId = static_cast<std::uint32_t>(packet.payload[0]) |
                                     (static_cast<std::uint32_t>(packet.payload[1]) << 8U) |
                                     (static_cast<std::uint32_t>(packet.payload[2]) << 16U) |
                                     (static_cast<std::uint32_t>(packet.payload[3]) << 24U);
            std::uint8_t nodeId = packet.payload[4];
            std::uint32_t rxTimestamp = static_cast<std::uint32_t>(packet.payload[5]) |
                                        (static_cast<std::uint32_t>(packet.payload[6]) << 8U) |
                                        (static_cast<std::uint32_t>(packet.payload[7]) << 16U) |
                                        (static_cast<std::uint32_t>(packet.payload[8]) << 24U);
            heartbeatAckManager_.onAckReceived(ackPktId, nodeId, rxTimestamp);
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
