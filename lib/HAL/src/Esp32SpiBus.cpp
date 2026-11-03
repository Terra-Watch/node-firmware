#include "Esp32SpiBus.h"
#include <SPI.h>
#include <LoRa.h>

namespace terrawatch::hal {
// The ESP32-S3 uses FSPI. On the classic ESP32 FSPI is the flash bus: build with -D TW_SPI_BUS=VSPI.
#ifndef TW_SPI_BUS
#define TW_SPI_BUS FSPI
#endif
static SPIClass loraSpi(TW_SPI_BUS);
void Esp32SpiBus::begin(const config::RadioConfig &config) {
    loraSpi.begin(config.sckPin, config.misoPin, config.mosiPin, config.chipSelectPin);
    LoRa.setSPI(loraSpi);
}
}
