#include <Arduino.h>

struct Settings
{
    String wifiSsid;
    String wifiPassword;
    String deviceName;
    int broadcastPort;
    int lightDuration;
    int broadcastInterval;
    int checkLightingLevelInterval;
    int checkVBatInterval;
    int checkEnvironmentInterval;
    int pirSensorCheckInterval;
    unsigned long masterListenDuration;
    float vdR1;
    float vdR2;
    float maxADC;
    float refVoltage;
    float vbatMin;
    float minLightLevel;
    String usrPassword;
    String usrName;
    unsigned int maxHumidityPoints = 10;
    unsigned int maxTemperaturePoints = 10;
    unsigned int maxVoltagePoints = 10;
    String toString();
};

struct Device
{
    String ip;
    String mac;
    String name;
    bool isMaster;
};

bool readSettings(Settings &settings, String settingsFile);