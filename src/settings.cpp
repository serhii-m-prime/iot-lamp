#include "settings.h"
#include <LittleFS.h>

bool readSettings(Settings &settings, String settingsFile)
{
    File file = LittleFS.open(settingsFile, "r");
    if (!file)
    {
        Serial.println("Failed to open settings file");
        return false;
    }

    while (file.available())
    {
        String line = file.readStringUntil('\n');
        line.trim();
        int separatorIndex = line.indexOf('=');
        if (separatorIndex > 0)
        {
            String key = line.substring(0, separatorIndex);
            String value = line.substring(separatorIndex + 1);

            if (key == "wifiSsid")
            {
                settings.wifiSsid = value;
            }
            else if (key == "wifiPassword")
            {
                settings.wifiPassword = value;
            }
            else if (key == "deviceName")
            {
                settings.deviceName = value;
            }
            else if (key == "broadcastPort")
            {
                settings.broadcastPort = value.toInt();
            }
            else if (key == "lightDuration")
            {
                settings.lightDuration = value.toInt();
            }
            else if (key == "broadcastInterval")
            {
                settings.broadcastInterval = value.toInt();
            }
            else if (key == "masterListenDuration")
            {
                settings.masterListenDuration = value.toInt();
            }
            else if (key == "vdR1")
            {
                settings.vdR1 = value.toFloat();
            }
            else if (key == "vdR2")
            {
                settings.vdR2 = value.toFloat();
            }
            else if (key == "maxADC")
            {
                settings.maxADC = value.toFloat();
            }
            else if (key == "refVoltage")
            {
                settings.refVoltage = value.toFloat();
            }
            else if (key == "vbatMin")
            {
                settings.vbatMin = value.toFloat();
            }
            else if (key == "checkLightingLevelInterval")
            {
                settings.checkLightingLevelInterval = value.toInt();
            }
            else if (key == "checkVBatInterval")
            {
                settings.checkVBatInterval = value.toInt();
            }
            else if (key == "checkEnvironmentInterval")
            {
                settings.checkEnvironmentInterval = value.toInt();
            }
            else if (key == "pirSensorCheckInterval") {
                settings.pirSensorCheckInterval = value.toInt();
            }
            else if (key == "minLightLevel") {
                settings.minLightLevel = value.toFloat();
            }
            else if (key == "usrPassword") {
                settings.usrPassword = value;
            }
            else if (key == "usrName") {
                settings.usrName = value;
            }
            else if (key == "maxHumidityPoints") {
                settings.maxHumidityPoints = value.toInt();
            }
            else if (key == "maxTemperaturePoints") {
                settings.maxTemperaturePoints = value.toInt();
            }
            else if (key == "maxVoltagePoints") {
                settings.maxVoltagePoints = value.toInt();
            }
             else {
                Serial.println("Unknown key: " + key);
            }
        }
    }
    file.close();
    return true;
}

String Settings::toString()
{
    String result = "wifiSsid=" + wifiSsid + "\n";
    result += "wifiPassword=" + wifiPassword + "\n";
    result += "deviceName=" + deviceName + "\n";
    result += "broadcastPort=" + String(broadcastPort) + "\n";
    result += "lightDuration=" + String(lightDuration) + "\n";
    result += "broadcastInterval=" + String(broadcastInterval) + "\n";
    result += "masterListenDuration=" + String(masterListenDuration) + "\n";
    result += "vdR1=" + String(vdR1) + "\n";
    result += "vdR2=" + String(vdR2) + "\n";
    result += "maxADC=" + String(maxADC) + "\n";
    result += "refVoltage=" + String(refVoltage) + "\n";
    result += "vbatMin=" + String(vbatMin) + "\n";
    result += "checkLightingLevelInterval=" + String(checkLightingLevelInterval) + "\n";
    result += "checkVBatInterval=" + String(checkVBatInterval) + "\n";
    result += "checkEnvironmentInterval=" + String(checkEnvironmentInterval) + "\n";
    result += "pirSensorCheckInterval=" + String(pirSensorCheckInterval) + "\n";
    result += "minLightLevel=" + String(minLightLevel) + "\n";
    result += "maxHumidityPoints=" + String(maxHumidityPoints) + "\n";
    result += "maxTemperaturePoints=" + String(maxTemperaturePoints) + "\n";
    result += "maxVoltagePoints=" + String(maxVoltagePoints) + "\n";
    return result;
}