#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include "PacketFactory.h"
#include "PacketQueue.h"
class BLECharacteristic;
namespace jalari::services {
class CrewMessageService {
public:
    CrewMessageService(protocol::PacketFactory&, queue::PacketQueue&);
    bool begin();
    bool accept(const std::uint8_t*, std::size_t);
private:
    struct Duplicate { std::array<std::uint8_t, 16> id{}; bool active = false; };
    protocol::PacketFactory& packetFactory_;
    queue::PacketQueue& packetQueue_;
    BLECharacteristic* characteristic_{nullptr};
    std::array<Duplicate, 16> duplicates_{};
    std::size_t nextDuplicate_{0};
    bool duplicate_(const std::uint8_t*);
};
}
