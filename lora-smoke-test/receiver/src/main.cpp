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
constexpr int kMaxPacketBytes = 255;

void logLine(const char* message) {
  Serial.println(message);
  Serial.flush();
}

void logf(const char* format, ...) {
  char buffer[256];
  va_list args;
  va_start(args, format);
  vsnprintf(buffer, sizeof(buffer), format, args);
  va_end(args);
  logLine(buffer);
}

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

void setup() {
  Serial.begin(115200);
  delay(1500);

  logLine("");
  logLine("Gateway SX1278 RF RX diagnostic");
  logLine("BOOT_MARKER: setup() started");
  logf("Pins SCK=%d MISO=%d MOSI=%d CS/NSS=%d RESET=%d DIO0=%d",
       kSck, kMiso, kMosi, kCs, kReset, kDio0);
  logf("Frequency: %ld Hz", kFrequencyHz);

  pinMode(kCs, OUTPUT);
  digitalWrite(kCs, HIGH);
  logLine("BOOT_MARKER: CS high");

  SPI.begin(kSck, kMiso, kMosi, kCs);
  logf("BOOT_MARKER: SPI.begin done; pre-LoRa REG_VERSION=0x%02X",
       readRegister(0x42U));

  LoRa.setPins(kCs, kReset, kDio0);
  logLine("BOOT_MARKER: LoRa pins set; calling LoRa.begin");

  if (!LoRa.begin(kFrequencyHz)) {
    logf("LoRa init failed; REG_VERSION=0x%02X", readRegister(0x42U));
    while (true) delay(1000);
  }

  logf("BOOT_MARKER: LoRa.begin success; REG_VERSION=0x%02X",
       readRegister(0x42U));

  LoRa.setSpreadingFactor(7);
  LoRa.setSignalBandwidth(125000L);
  LoRa.setCodingRate4(5);
  LoRa.setSyncWord(0x12);
  LoRa.enableCrc();
  LoRa.receive();

  logf("LoRa Receiver Ready; REG_VERSION=0x%02X", readRegister(0x42U));
  logLine("Settings: 433MHz SF7 BW125 CR4/5 SW0x12 CRC on");
}

char printableAscii(std::uint8_t value) {
  if (value >= 32 && value <= 126) return static_cast<char>(value);
  return '.';
}

void loop() {
  const int packetSize = LoRa.parsePacket();
  if (packetSize <= 0) return;

  std::uint8_t bytes[kMaxPacketBytes];
  int count = 0;
  while (LoRa.available() && count < kMaxPacketBytes) {
    bytes[count++] = static_cast<std::uint8_t>(LoRa.read());
  }

  logLine("--------------------------------");
  logf("RX packetSize=%d bytesRead=%d RSSI=%d dBm SNR=%.2f dB",
       packetSize, count, LoRa.packetRssi(), LoRa.packetSnr());

  if (packetSize != count) {
    logf("WARN: packetSize/read mismatch packetSize=%d bytesRead=%d", packetSize, count);
  }
  if (packetSize == 25) {
    logLine("NOTE: length 25 matches expected node heartbeat size");
  } else if (packetSize == 21) {
    logLine("NOTE: length 21 matches expected Gateway heartbeat ACK size");
  } else {
    logLine("NOTE: length does not match expected heartbeat/ACK size");
  }

  Serial.print("HEX:");
  for (int i = 0; i < count; ++i) {
    Serial.printf(" %02X", bytes[i]);
  }
  Serial.println();

  Serial.print("ASCII:");
  for (int i = 0; i < count; ++i) {
    Serial.print(printableAscii(bytes[i]));
  }
  Serial.println();
  Serial.flush();
}
