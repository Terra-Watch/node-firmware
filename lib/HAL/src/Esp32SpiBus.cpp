#include "Esp32SpiBus.h"
#include <SPI.h>
#include <LoRa.h>

namespace jalari::hal {
static SPIClass loraSpi(FSPI);
void Esp32SpiBus::begin(const config::RadioConfig &config) {
    loraSpi.begin(config.sckPin, config.misoPin, config.mosiPin, config.chipSelectPin);
    LoRa.setSPI(loraSpi);
}
}
