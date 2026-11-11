#include <Arduino.h>
#include <SPI.h>
#include <LoRa.h>

namespace {
constexpr int kSck = 12;
constexpr int kMiso = 13;
constexpr int kMosi = 11;
constexpr int kCs = 10;
constexpr int kReset = 9;
constexpr int kDio0 = 14;
constexpr long kFrequencyHz = 433E6;

std::uint8_t readRegister(std::uint8_t address) {
  SPI.beginTransaction(SPISettings(200000U, MSBFIRST, SPI_MODE0));
  digitalWrite(kCs, LOW);
  SPI.transfer(address & 0x7FU);
  const std::uint8_t value = SPI.transfer(0x00U);
  digitalWrite(kCs, HIGH);
  SPI.endTransaction();
  return value;
}
}

int counter = 0;

void setup() {
  Serial.begin(115200);
  delay(500);

  pinMode(kCs, OUTPUT);
  digitalWrite(kCs, HIGH);
  SPI.begin(kSck, kMiso, kMosi, kCs);
  LoRa.setPins(kCs, kReset, kDio0);

  if (!LoRa.begin(kFrequencyHz)) {
    Serial.printf("LoRa init failed; REG_VERSION=0x%02X\n", readRegister(0x42U));
    while (true) delay(1000);
  }

  LoRa.setSpreadingFactor(7);
  LoRa.setSignalBandwidth(125000L);
  LoRa.setCodingRate4(5);
  LoRa.setSyncWord(0x12);
  LoRa.enableCrc();

  Serial.printf("LoRa Transmitter Ready; REG_VERSION=0x%02X\n", readRegister(0x42U));
  Serial.println("Settings: 433MHz SF7 BW125 CR4/5 SW0x12 CRC on");
}

void loop() {
  Serial.printf("Sending diagnostic packet: %d\n", counter);
  LoRa.beginPacket();
  LoRa.printf("Hello %d", counter++);
  const int result = LoRa.endPacket();
  Serial.printf("TX result: %s\n", result == 1 ? "success" : "failure");
  delay(2000);
}
