#ifndef LORAMANAGER_H
#define LORAMANAGER_H

#include <Arduino.h>

class LoRaManager
{
public:

    bool begin();

    bool sendPacket(const String &msg);

};

#endif