#include <LittleFS.h>
#include <ArduinoJson.h>

namespace HumidityLogger
{
    void addNewPoint(float value);
    String getAllPoints();
}