#include "Esp32SerialConsole.h"
#include <Arduino.h>
namespace jalari::hal { void Esp32SerialConsole::begin(unsigned long baudRate) { Serial.begin(baudRate); } void Esp32SerialConsole::print(const char *text) { Serial.print(text); } void Esp32SerialConsole::println(const char *text) { Serial.println(text); } }
