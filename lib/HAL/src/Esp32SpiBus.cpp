#include "Esp32SpiBus.h"
#include <SPI.h>
namespace jalari::hal { void Esp32SpiBus::begin(const config::RadioConfig &config) { SPI.begin(config.sckPin, config.misoPin, config.mosiPin); } }
