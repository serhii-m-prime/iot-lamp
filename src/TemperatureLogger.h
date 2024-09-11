#include <LittleFS.h>
#include <ArduinoJson.h>

namespace TemperatureLogger
{
    void addNewPoint(float value);
    String getAllPoints();
}