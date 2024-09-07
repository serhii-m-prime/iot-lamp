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

            if (key == "wifi_ssid")
            {
                settings.wifi_ssid = value;
            }
            else if (key == "wifi_password")
            {
                settings.wifi_password = value;
            }
            else if (key == "device_name")
            {
                settings.device_name = value;
            }
            else if (key == "broadcast_port")
            {
                settings.broadcast_port = value.toInt();
            }
            else if (key == "light_duration")
            {
                Serial.println("light_duration: " + value);
                settings.light_duration = value.toInt() * 1000;
            }
            else if (key == "broadcast_interval")
            {
                Serial.println("broadcast_interval: " + value);
                settings.broadcast_interval = value.toInt() * 1000;
            }
            else if (key == "listen_duration")
            {
                Serial.println("master_listen_duration: " + value);
                settings.master_listen_duration = value.toInt() * 1000;
            }
            else if (key == "R1")
            {
                Serial.println("R1: " + value);
                settings.R1 = value.toFloat();
            }
            else if (key == "R2")
            {
                Serial.println("R2: " + value);
                settings.R2 = value.toFloat();
            }
            else if (key == "maxADC")
            {
                Serial.println("maxADC: " + value);
                settings.maxADC = value.toFloat();
            }
            else if (key == "refVoltage")
            {
                Serial.println("refVoltage: " + value);
                settings.refVoltage = value.toFloat();
            }
            else if (key == "vbatMin")
            {
                Serial.println("vbatMin: " + value);
                settings.vbatMin = value.toFloat();
            }
        }
    }
    file.close();
    return true;
}