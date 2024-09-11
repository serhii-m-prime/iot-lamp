#include <LittleFS.h>
#include <ArduinoJson.h>

namespace VoltageLogger
{
    void addNewPoint(float value);
    String getAllPoints();
}