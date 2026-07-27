#include <Arduino.h>

#include "lora/LoRaManager.h"

LoRaManager radio;

void setup()
{
    Serial.begin(115200);

    delay(2000);

    Serial.println("================================");
    Serial.println("BoatNode Transmitter");
    Serial.println("================================");

    if(!radio.begin())
    {
        Serial.println("LoRa Init Failed");

        while(true);
    }

    Serial.println("LoRa Ready");
}

void loop()
{
    radio.sendPacket("BoatNode Test Packet");

    Serial.println("Packet Sent");

    delay(2000);
}