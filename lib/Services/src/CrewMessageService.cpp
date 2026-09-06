#include "CrewMessageService.h"
#include <algorithm>
#include <BLE2902.h>
#include <BLECharacteristic.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
namespace jalari::services { namespace {
constexpr char kServiceUuid[] = "0000f100-0000-1000-8000-00805f9b34fb";
constexpr char kCharacteristicUuid[] = "0000f101-0000-1000-8000-00805f9b34fb";
constexpr std::uint8_t kMagic[] = {0x4A, 0x43, 0x4D, 0x31};
class WriteCallbacks final : public BLECharacteristicCallbacks {
public: explicit WriteCallbacks(CrewMessageService& s) : service_(s) {}
    void onWrite(BLECharacteristic* c) override { std::string v = c->getValue(); const bool ok = service_.accept(reinterpret_cast<const std::uint8_t*>(v.data()), v.size()); c->setValue(ok ? "BOATNODE_RECEIVED" : "REJECTED"); c->notify(); }
private: CrewMessageService& service_;
}; }
CrewMessageService::CrewMessageService(protocol::PacketFactory& f, queue::PacketQueue& q) : packetFactory_(f), packetQueue_(q) {}
bool CrewMessageService::begin() {
    BLEDevice::init("JALRI-BoatNode");
    BLEServer* server = BLEDevice::createServer();
    BLEService* service = server->createService(kServiceUuid);
    characteristic_ = service->createCharacteristic(kCharacteristicUuid, BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_NOTIFY);
    characteristic_->addDescriptor(new BLE2902());
    characteristic_->setCallbacks(new WriteCallbacks(*this));
    service->start(); BLEDevice::getAdvertising()->addServiceUUID(kServiceUuid); BLEDevice::getAdvertising()->start(); return true;
}
bool CrewMessageService::duplicate_(const std::uint8_t* payload) {
    for (const Duplicate& item : duplicates_) if (item.active && std::equal(item.id.begin(), item.id.end(), payload + 4)) return true;
    auto& item = duplicates_[nextDuplicate_]; std::copy(payload + 4, payload + 20, item.id.begin()); item.active = true; nextDuplicate_ = (nextDuplicate_ + 1U) % duplicates_.size(); return false;
}
bool CrewMessageService::accept(const std::uint8_t* payload, std::size_t size) {
    if (payload == nullptr || size < 21U || size > 48U || !std::equal(std::begin(kMagic), std::end(kMagic), payload)) return false;
    const std::size_t textSize = payload[20]; if (textSize == 0U || textSize > 27U || size != 21U + textSize || duplicate_(payload)) return false;
    protocol::Packet packet; if (!packetFactory_.createData(constants::kGatewayNodeId, payload, size, packet)) return false; return packetQueue_.enqueue(packet);
}
}
