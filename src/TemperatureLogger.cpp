#include "TemperatureLogger.h"
#include <LittleFS.h>
#include <ArduinoJson.h>
#include "settings.h"
#include <NTPClient.h>

extern Settings settings;
extern NTPClient timeClient;

const String fileName = "/temperature.json";

void TemperatureLogger::addNewPoint(float value)
{
    long timestamp = timeClient.getEpochTime();
    if (timestamp < 1726000000) {
        return;
    }
    String fileContent;
    File file = LittleFS.open(fileName, "r");
    if (file)
    {
        fileContent = file.readString();
        if (!fileContent || fileContent.length() == 0)
        {
            fileContent = "[]";
        }
        file.close();
    }
    else
    {
        fileContent = "[]";
        file = LittleFS.open(fileName, "w");
        file.print(fileContent);
        file.close();
    }
    JsonDocument doc;
    JsonArray records;
    if (fileContent.length() > 0)
    {
        DeserializationError error = deserializeJson(doc, fileContent);
        if (error)
        {
            Serial.print("Failed to deserialize JSON: ");
            Serial.println(error.c_str());
        }
        else
        {
            records = doc.as<JsonArray>();
        }
    }
    else
    {
        records = doc.to<JsonArray>();
    }

    // Add new record
    if (records.size() >= settings.maxTemperaturePoints)
    {
        records.remove(0); // Remove the oldest record if the limit is exceeded
    }

    JsonArray newRecord = records.createNestedArray();
    newRecord.add(timestamp);
    newRecord.add(value);

    // Write updated records back to file
    file = LittleFS.open(fileName, "w");
    if (file)
    {
        String fileContent = "";
        serializeJson(doc, fileContent);
        file.print(fileContent);
        file.close();
    }
    else
    {
        Serial.println("Failed to open file for writing");
    }
}

String TemperatureLogger::getAllPoints()
{
    if (!LittleFS.exists(fileName))
    {
        return "[]";
    }
    File file = LittleFS.open(fileName, "r");
    String result = file.readString();
    return result;
}