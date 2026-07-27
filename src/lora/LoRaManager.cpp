#include "LoRaManager.h"

#include <SPI.h>
#include <LoRa.h>

#define LORA_SCK   12
#define LORA_MISO  13
#define LORA_MOSI  11
#define LORA_CS    10
#define LORA_RST    9
#define LORA_DIO0  14

bool LoRaManager::begin()
{
    SPI.begin(LORA_SCK, LORA_MISO, LORA_MOSI, LORA_CS);

    LoRa.setPins(LORA_CS, LORA_RST, LORA_DIO0);

    if(!LoRa.begin(433E6))
    {
        return false;
    }

    return true;
}

bool LoRaManager::sendPacket(const String &msg)
{
    LoRa.beginPacket();

    LoRa.print(msg);

    return LoRa.endPacket();
}