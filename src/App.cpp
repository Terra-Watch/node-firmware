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
#include "RouteAdvertisementService.h"
#include "RouteFailureService.h"
#include "DtnStoreForwardService.h"
#include "PacketFactory.h"
#include "TestCommandParser.h"
#include "TelemetryService.h"

namespace jalari
{
    App::App(radio::IRadio &radio, node::NodeManager &node, queue::PacketQueue &packetQueue, event::EventBus &eventBus,
             services::HeartbeatService &heartbeatService, services::HeartbeatAckManager &heartbeatAckManager,
             services::NeighborService &neighborService, services::ReliableLinkService &reliableLinkService,
             services::TelemetryService &telemetryService,
             services::ForwardingService &forwardingService, routing::RouteAdvertisementService &routeAdvertisementService,
             routing::RouteFailureService &routeFailureService, services::DtnStoreForwardService &dtnService,
             utils::Logger &logger, protocol::PacketFactory &packetFactory, hal::IConsole &console)
        : radio_(radio), node_(node), packetQueue_(packetQueue), eventBus_(eventBus), heartbeatService_(heartbeatService),
          heartbeatAckManager_(heartbeatAckManager), neighborService_(neighborService),
          reliableLinkService_(reliableLinkService), telemetryService_(telemetryService), forwardingService_(forwardingService),
          routeAdvertisementService_(routeAdvertisementService), routeFailureService_(routeFailureService),
          dtnService_(dtnService), logger_(logger), packetFactory_(packetFactory), console_(console) {}

    bool App::begin()
    {
        forwardingService_.setDtnService(&dtnService_);
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
        if (!routeAdvertisementService_.begin(eventBus_))
        {
            LOG_ERROR(logger_, "Route advertisement service subscription failed");
            return false;
        }
        if (!routeFailureService_.begin(eventBus_))
        {
            LOG_ERROR(logger_, "Route failure service subscription failed");
            return false;
        }
        if (!radio_.begin())
        {
            node_.setStatus(node::NodeStatus::Fault);
            eventBus_.publish({event::Type::RadioError});
            return false;
        }
        if (!telemetryService_.begin())
        {
            node_.setStatus(node::NodeStatus::Fault);
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
        processTestCommands_();
        reliableLinkService_.update();
        heartbeatService_.update();
        telemetryService_.update();
        heartbeatAckManager_.update();
        neighborService_.update();
        routeAdvertisementService_.update();
        dtnService_.update();
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
            LOG_INFO(logger_, "[HEARTBEAT TX]");

            char msg[64];
            snprintf(msg, sizeof(msg), "Node ID: %u", packet->sourceId);
            LOG_INFO(logger_, msg);

            snprintf(msg, sizeof(msg), "type: %u", static_cast<unsigned>(packet->type));
            LOG_INFO(logger_, msg);
            snprintf(msg, sizeof(msg), "Destination: 0x%02X", packet->destinationId);
            LOG_INFO(logger_, msg);

            snprintf(msg, sizeof(msg), "Sequence: %u", packet->sequenceNumber);
            LOG_INFO(logger_, msg);

            std::uint32_t packetId = 0;
            if (packet->payloadSize >= 4)
            {
                packetId = static_cast<std::uint32_t>(packet->payload[0]) |
                           (static_cast<std::uint32_t>(packet->payload[1]) << 8U) |
                           (static_cast<std::uint32_t>(packet->payload[2]) << 16U) |
                           (static_cast<std::uint32_t>(packet->payload[3]) << 24U);
            }
            snprintf(msg, sizeof(msg), "Packet ID: %lu", static_cast<unsigned long>(packetId));
            LOG_INFO(logger_, msg);

            snprintf(msg, sizeof(msg), "Packet Size: %u", static_cast<unsigned int>(bufferSize));
            LOG_INFO(logger_, msg);

            std::uint32_t uptimeMs = 0;
            if (packet->payloadSize >= 8)
            {
                uptimeMs = static_cast<std::uint32_t>(packet->payload[4]) |
                           (static_cast<std::uint32_t>(packet->payload[5]) << 8U) |
                           (static_cast<std::uint32_t>(packet->payload[6]) << 16U) |
                           (static_cast<std::uint32_t>(packet->payload[7]) << 24U);
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
            reliableLinkService_.onTransmissionFailed(failedPacket);
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
            LOG_INFO(logger_, "[WAITING ACK]");
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

        if (packet.type == protocol::PacketType::HeartbeatAck)
        {
            if (packet.payloadSize != 9 || packet.destinationId != node_.id() ||
                packet.payload[4] != node_.id())
            {
                LOG_WARN(logger_, "[HEARTBEAT ACK REJECTED] Wrong target or payload");
                return;
            }
            heartbeatAckManager_.onAckReceived(packet);
            return;
        }

        if (packet.type == protocol::PacketType::Data)
        {
            if (!reliableLinkService_.processReceivedData(packet)) return;
        }
        if (forwardingService_.processReceived(packet))
            eventBus_.publish({event::Type::PacketReceived, packet, frame.rssi, frame.snr});
    }

    void App::processTestCommands_()
    {
#if JALARI_TEST_MODE
        if (testRemainingCount_ > 0U)
        {
            if (!packetQueue_.empty() || !reliableLinkService_.readyForTestPacket()) return;
            protocol::Packet packet;
            if (packetFactory_.createData(testDestinationId_, testPayload_, testPayloadSize_, packet) &&
                packetQueue_.enqueue(packet))
            {
                --testRemainingCount_;
                char message[96];
                snprintf(message, sizeof(message), "TEST_DATA_QUEUED dst=0x%02X seq=%u remaining=%u",
                         testDestinationId_, packet.sequenceNumber, testRemainingCount_);
                LOG_INFO(logger_, message);
            }
            return;
        }
        char line[constants::kTestCommandLineCapacity]{};
        if (!console_.readLine(line, sizeof(line))) return;
        services::TestCommand command;
        if (!services::parseTestCommand(line, command))
        {
            LOG_WARN(logger_, "TEST_CMD_ERROR");
            return;
        }
        if (command.destinationId == node_.id())
        {
            LOG_WARN(logger_, "TEST_CMD_ERROR_LOCAL_DESTINATION");
            return;
        }
        testDestinationId_ = command.destinationId;
        testRemainingCount_ = command.count;
        testPayloadSize_ = command.payloadSize;
        for (std::size_t index = 0; index < testPayloadSize_; ++index) testPayload_[index] = command.payload[index];
#endif
    }

    void App::onEvent(const event::Event &event)
    {
        switch (event.type)
        {
        case event::Type::PacketSent:
        case event::Type::PacketReceived:
        {
            char msg[192];
            const char *direction = event.type == event::Type::PacketSent ? "TX" : "RX";
            std::uint16_t acknowledgedSequence = 0U;
            if (event.packet.type == protocol::PacketType::Acknowledgement && event.packet.payloadSize >= 2U)
                acknowledgedSequence = static_cast<std::uint16_t>(event.packet.payload[0]) |
                                       (static_cast<std::uint16_t>(event.packet.payload[1]) << 8U);
            snprintf(msg, sizeof(msg), "[RF %s] type=%u source=0x%02X destination=0x%02X previousHop=0x%02X sequence=%u ackSequence=%u ttl=%u hops=%u payloadSize=%u rssi=%d snr=%.2f",
                     direction, static_cast<unsigned>(event.packet.type), event.packet.sourceId,
                     event.packet.destinationId, event.packet.previousHopId, event.packet.sequenceNumber,
                     acknowledgedSequence, event.packet.ttl, event.packet.hopCount,
                     static_cast<unsigned>(event.packet.payloadSize),
                     event.rssi, static_cast<double>(event.snr));
            LOG_INFO(logger_, msg);
            break;
        }
        case event::Type::AcknowledgementReceived:
        {
            char msg[64];
            const std::uint16_t sequence = event.packet.payloadSize >= 2U
                ? static_cast<std::uint16_t>(event.packet.payload[0]) |
                  (static_cast<std::uint16_t>(event.packet.payload[1]) << 8U) : 0U;
            snprintf(msg, sizeof(msg), "ACK_RX seq=%u source=0x%02X", sequence, event.packet.sourceId);
            LOG_INFO(logger_, msg);
            break;
        }
        case event::Type::AcknowledgementFailed:
        {
            char msg[64];
            snprintf(msg, sizeof(msg), "ACK_FAILED seq=%u", event.packet.sequenceNumber);
            LOG_WARN(logger_, msg);
            break;
        }
        case event::Type::TransmissionRetryQueued:
        {
            char msg[64];
            snprintf(msg, sizeof(msg), "TX_RETRY seq=%u", event.packet.sequenceNumber);
            LOG_WARN(logger_, msg);
            break;
        }
        case event::Type::TransmissionFailed:
            LOG_ERROR(logger_, "TX_FAILED");
            break;
        case event::Type::HeartbeatSent: LOG_INFO(logger_, "Heartbeat packet sent"); break;
        case event::Type::RadioReady: LOG_INFO(logger_, "LoRa ready"); break;
        case event::Type::RadioError: LOG_ERROR(logger_, "LoRa initialization failed"); break;
        default: break;
        }
    }
} // namespace jalari
