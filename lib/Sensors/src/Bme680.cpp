#include "Bme680.h"

#include <Adafruit_BME680.h>
#include <Wire.h>

namespace terrawatch::sensors
{
    namespace
    {
        Adafruit_BME680 device(&Wire);
    }

    bool Bme680::begin(int sdaPin, int sclPin)
    {
        if (!Wire.begin(sdaPin, sclPin)) return false;
        if (!device.begin(0x76) && !device.begin(0x77)) return false; // SDO to GND = 0x76, to VCC = 0x77
        device.setTemperatureOversampling(BME680_OS_8X);
        device.setHumidityOversampling(BME680_OS_2X);
        device.setPressureOversampling(BME680_OS_NONE);
        device.setIIRFilterSize(BME680_FILTER_SIZE_0);
        device.setGasHeater(0, 0);
        return true;
    }

    bool Bme680::read(Bme680Sample &sample)
    {
        if (!device.performReading()) return false;
        sample.temperatureCentiC = static_cast<std::int16_t>(device.temperature * 100.0F);
        sample.humidityCentiPercent = static_cast<std::uint16_t>(device.humidity * 100.0F);
        return true;
    }
} // namespace terrawatch::sensors
