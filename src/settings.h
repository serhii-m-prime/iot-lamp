#include <Arduino.h>

struct Settings
{
    String wifi_ssid;
    String wifi_password;
    String device_name;
    int broadcast_port;
    int light_duration;
    int broadcast_interval;
    int master_listen_duration;
    float R1;
    float R2;
    float maxADC;
    float refVoltage;
    float vbatMin;
};

bool readSettings(Settings &settings, String settingsFile);