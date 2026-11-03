#include "App.h"

#ifndef TW_HEARTBEAT
#define TW_HEARTBEAT 1 // 0 = do not transmit heartbeats
#endif
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

namespace terrawatch
{
    App::App(radio::IRadio &radio, node::NodeManager &node, queue::PacketQueue &packetQueue, event::EventBus &eventBus,
             services::HeartbeatService &heartbeatService, services::HeartbeatAckManager &heartbeatAckManager,
             services::NeighborService &neighborService, services::ReliableLinkService &reliableLinkService,
             services::TelemetryService *telemetryService,
             services::ForwardingService &forwardingService, routing::RouteAdvertisementService &routeAdvertisementService,
             routing::RouteFailureService &routeFailureService, services::DtnStoreForwardService &dtnService,
             utils::Logger &logger,
             protocol::PacketFactory &packetFactory, hal::IConsole &console)
        : radio_(radio), node_(node), packetQueue_(packetQueue), eventBus_(eventBus), heartbeatService_(heartbeatService),
          heartbeatAckManager_(heartbeatAckManager), neighborService_(neighborService),
          reliableLinkService_(reliableLinkService), telemetryService_(telemetryService), forwardingService_(forwardingService),
          routeAdvertisementService_(routeAdvertisementService), routeFailureService_(routeFailureService),
          dtnService_(dtnService), logger_(logger),
          packetFactory_(packetFactory), console_(console) {}

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
        if (telemetryService_ != nullptr && !telemetryService_->begin())
        {
            node_.setStatus(node::NodeStatus::Fault);
            return false;
        }
        node_.setStatus(node::NodeStatus::Ready);
        initialized_ = true;
        eventBus_.publish({event::Type::RadioReady});
        return true;
    }

    // The board's RGB LED shows what the node just sent: blue = air-quality reading, red = chainsaw alert, magenta = blast alert, white = position.
    // TW_LED_PIN is the pin of the WS2812 on the board (48 on most ESP32-S3 DevKitC-1, 38 on v1.1); undefined = no LED.
    void App::flashLed_(std::uint8_t red, std::uint8_t green, std::uint8_t blue, std::uint32_t durationMs)
    {
#ifdef TW_LED_PIN
        neopixelWrite(TW_LED_PIN, red, green, blue);
        ledOffMs_ = millis() + durationMs; if (ledOffMs_ == 0U) ledOffMs_ = 1U;
#else
        (void)red; (void)green; (void)blue; (void)durationMs;
#endif
    }

    void App::update()
    {
        if (!initialized_) return;
#ifdef TW_LED_PIN
        if (ledOffMs_ != 0U && static_cast<std::int32_t>(millis() - ledOffMs_) >= 0) { neopixelWrite(TW_LED_PIN, 0, 0, 0); ledOffMs_ = 0U; }
#endif
        processIncoming_();
        processTestCommands_();
        reliableLinkService_.update();
#if TW_HEARTBEAT
        heartbeatService_.update();
#endif
        {
            protocol::Packet again;
            if (reliableLinkService_.nextEndToEndRetry(millis(), again) && packetQueue_.enqueue(again))
            {
                char msg[64];
                snprintf(msg, sizeof(msg), "[E2E RETRY] seq=%u not confirmed by the gateway, sending again", again.sequenceNumber);
                LOG_WARN(logger_, msg);
            }
        }
        if (telemetryService_ != nullptr) telemetryService_->update();
#if TW_HEARTBEAT
        heartbeatAckManager_.update();
#endif
        neighborService_.update();
        routeAdvertisementService_.update();
        dtnService_.update();
        transmitQueued_();
    }

    void App::transmitQueued_()
    {
        const protocol::Packet *packet = packetQueue_.peek();
        if (packet == nullptr) return;
        // Name the next hop at the moment of sending, from the route table as it is now (a retry after a failover goes to the backup parent).
        protocol::Packet outgoing = *packet;
        if (outgoing.type == protocol::PacketType::Data || outgoing.type == protocol::PacketType::EndToEndAck)
            forwardingService_.resolveNextHop(outgoing);
        packet = &outgoing;

        // Do not start the first attempt at once: nodes that heard the same packet would answer together. Then listen before talking.
        const std::uint32_t nowMs = millis();
        if (!txJitterArmed_)
        {
            txJitterArmed_ = true; lbtAttempts_ = 0U;
            txNotBeforeMs_ = nowMs + static_cast<std::uint32_t>(random(0, static_cast<long>(constants::kTxJitterMaxMs) + 1L));
        }
        if (static_cast<std::int32_t>(nowMs - txNotBeforeMs_) < 0) return;
        if (lbtAttempts_ < constants::kLbtMaxAttempts && radio_.channelBusy())
        {
            const std::uint32_t window = constants::kLbtBackoffBaseMs << lbtAttempts_;
            ++lbtAttempts_; ++lbtBusyCount_;
            txNotBeforeMs_ = nowMs + static_cast<std::uint32_t>(random(1, static_cast<long>(window) + 1L));
            return;
        }
        if (lbtAttempts_ > 0U)
        {
            char msg[64];
            snprintf(msg, sizeof(msg), "[LBT] channel busy %u time(s) before this send, total %lu", lbtAttempts_, static_cast<unsigned long>(lbtBusyCount_));
            LOG_INFO(logger_, msg);
        }
        txJitterArmed_ = false;

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
            failedPacket.nextHopId = outgoing.nextHopId;
            reliableLinkService_.onTransmissionFailed(failedPacket);
            return;
        }

        protocol::Packet sentPacket;
        (void)packetQueue_.dequeue(sentPacket);
        sentPacket.nextHopId = outgoing.nextHopId;
        if (sentPacket.type == protocol::PacketType::Data && sentPacket.sourceId == node_.id() && sentPacket.payloadSize > 0U)
        {
            const std::uint8_t kind = sentPacket.payload[0];
            if (kind == 0x53U) flashLed_(0, 0, 40, 300);                                                     // 'S' air-quality reading
            else if (kind == 0x41U) { if (sentPacket.payloadSize > 2U && sentPacket.payload[2] == 2U) flashLed_(40, 0, 40, 1500); else flashLed_(60, 0, 0, 1500); } // 'A' alert
            else if (kind == 0x50U) flashLed_(25, 25, 25, 300);                                              // 'P' position
        }
        // Only alerts are kept for an end-to-end resend. A periodic reading is replaced by the next one, and resending those loads the channel enough
        // to lose more than it saves (simulations/multihop).
        if (sentPacket.type == protocol::PacketType::Data && sentPacket.sourceId == node_.id() && sentPacket.destinationId == constants::kGatewayNodeId &&
            sentPacket.payloadSize > 0U && sentPacket.payload[0] == 0x41U)
            reliableLinkService_.trackEndToEnd(sentPacket, millis());
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

        // Protocol v5 names the next hop. A packet for someone else that does not name this node was only overheard: it is not ours to carry or to acknowledge.
        if (packet.destinationId != node_.id() && packet.destinationId != 0xFFU && packet.nextHopId != node_.id() &&
            packet.nextHopId != 0U && packet.nextHopId != constants::kAnyNextHop)
            return;

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

        if (packet.type == protocol::PacketType::EndToEndAck)
        {
            // Two relays that both hear a receipt must not pass it back and forth: forward each one once.
            if (reliableLinkService_.isDuplicate(packet.sourceId, packet.sequenceNumber)) return;
            if (packet.destinationId == node_.id())
            {
                const std::uint16_t acknowledged = static_cast<std::uint16_t>(packet.payload[0]) | (static_cast<std::uint16_t>(packet.payload[1]) << 8U);
                std::uint8_t retries = 0U;
                char msg[96];
                if (reliableLinkService_.confirmEndToEnd(acknowledged, retries))
                    snprintf(msg, sizeof(msg), "[E2E ACK] seq=%u reached the gateway after %u retr%s", acknowledged, retries, retries == 1U ? "y" : "ies");
                else
                    snprintf(msg, sizeof(msg), "[E2E ACK] seq=%u (already confirmed or not ours)", acknowledged);
                LOG_INFO(logger_, msg);
                return;
            }
        }
        if (packet.type == protocol::PacketType::Data)
        {
            (void)routeAdvertisementService_.learnReverseRoute(packet, millis());
            if (!reliableLinkService_.processReceivedData(packet)) return;
        }
        if (forwardingService_.processReceived(packet))
            eventBus_.publish({event::Type::PacketReceived, packet, frame.rssi, frame.snr});
    }

    void App::processTestCommands_()
    {
#if TW_TEST_MODE
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
            // The gateway acknowledged the packet itself (no relay in between): that is also the end-to-end receipt.
            std::uint8_t retries = 0U;
            if (event.packet.sourceId == constants::kGatewayNodeId && reliableLinkService_.confirmEndToEnd(sequence, retries))
            {
                snprintf(msg, sizeof(msg), "[E2E ACK] seq=%u reached the gateway directly (%u retries)", sequence, retries);
                LOG_INFO(logger_, msg);
            }
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
} // namespace terrawatch
