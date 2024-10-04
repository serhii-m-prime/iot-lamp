#include <Arduino.h>
#include <ArduinoOTA.h>
#include <ArduinoJson.h>
#include <ESP8266WiFi.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <WebHandler.h>
#include <LittleFS.h>
#include <WiFiClient.h>
#include <WiFiUdp.h>
#include <NTPClient.h>
#include <ESP8266HTTPClient.h>
#include "led.h"
#include "settings.h"
#include <Adafruit_AHTX0.h>
#include <BH1750.h>
#include <Wire.h>
#include "HumidityLogger.h"
#include "TemperatureLogger.h"
#include "VoltageLogger.h"

#define MOSFET_LED_PIN 13 // D1
#define PIR_PIN 4        // D2
#define BATTERY_PIN A0
#define MOSFET_LIGHT_MODULE_PIN 5 // D8
#define I2C_SDA_PIN 14             // D5
#define I2C_SCL_PIN 12             // D6

const char *settingsFile = "/settings.txt";

bool isMaster = false;
bool isLightOn = false;
bool isBatteryLow = false;
bool isLightOffByUserConmmand = false;
bool isLightOnByUserConmmand = false;
bool isLightModuleActive = false;
int prevPirState = LOW;
unsigned long lastBroadcastMsgTime = 0;
unsigned long lastMasterUpdateTime = 0;
unsigned long lastMoveTime = 0;
unsigned long lastAHTUpdate = 0;
unsigned long lastBatteryCheckTime = 0;
unsigned long lastMotionCheckTime = 0;
unsigned long lastLightLevelCheck = 0;
float cvBat = 0;
float cTemperature = 0;
float cHumidity = 0;
float cLight = 0;
std::vector<Device> devices;
Settings settings;

WiFiUDP udp;
IPAddress broadcastIP;
IPAddress localIP;
IPAddress masterIP;
AsyncWebServer server(80);
WiFiUDP ntcudp;
NTPClient timeClient(ntcudp, "europe.pool.ntp.org");
Adafruit_AHTX0 AHT10;
BH1750 lightMeter(0x23);

// List of uploaded files
void listFiles()
{
    Serial.println("");
    Dir dir = LittleFS.openDir("/");
    while (dir.next())
    {
        Serial.print(dir.fileName());
        Serial.print(" - ");
        Serial.println(dir.fileSize());
    }
}

IPAddress calculateBroadcastAddress(IPAddress ip, IPAddress subnet)
{
    IPAddress broadcast;
    for (int i = 0; i < 4; i++)
    {
        broadcast[i] = ip[i] | ~subnet[i];
    }
    return broadcast;
}

void connectToWiFi()
{
    Serial.print("Connecting to Wi-Fi: ");
    Serial.println(settings.wifiSsid);

    WiFi.setHostname(("ESP8266:" + settings.deviceName).c_str());
    WiFi.begin(settings.wifiSsid.c_str(), settings.wifiPassword.c_str());

    while (WiFi.status() != WL_CONNECTED)
    {
        ledOn();
        delay(250);
        ledOff();
        delay(250);
        Serial.print(".");
    }

    Serial.println();
    Serial.print("Connected to Wi-Fi. IP address: ");
    Serial.println(WiFi.localIP());
    localIP = WiFi.localIP();
    IPAddress subnetMask = WiFi.subnetMask();
    broadcastIP = calculateBroadcastAddress(localIP, subnetMask);
    Serial.println("Broadcast IP address: " + broadcastIP.toString());
}

void setupOTA()
{
    ArduinoOTA.setHostname("esp8266-ota");
    ArduinoOTA.onStart([]()
                       {
        String type;
        if (ArduinoOTA.getCommand() == U_FLASH) {
            type = "sketch";
        } else {
            type = "filesystem";
        }
        Serial.println("Start updating " + type); });

    ArduinoOTA.onEnd([]()
                     { Serial.println("\nEnd"); });

    ArduinoOTA.onProgress([](unsigned int progress, unsigned int total)
                          { Serial.printf("Progress: %u%%\r", (progress / (total / 100))); });

    ArduinoOTA.onError([](ota_error_t error)
                       {
        Serial.printf("Error[%u]: ", error);
        if (error == OTA_AUTH_ERROR) {
            Serial.println("Auth Failed");
        } else if (error == OTA_BEGIN_ERROR) {
            Serial.println("Begin Failed");
        } else if (error == OTA_CONNECT_ERROR) {
            Serial.println("Connect Failed");
        } else if (error == OTA_RECEIVE_ERROR) {
            Serial.println("Receive Failed");
        } else if (error == OTA_END_ERROR) {
            Serial.println("End Failed");
        } });

    ArduinoOTA.begin();
}

void initLittleFS()
{
    if (!LittleFS.begin())
    {
        Serial.println("Failed to mount LittleFS.");
    }
    else
    {
        Serial.println("LittleFS mounted.");
        listFiles();
        // Get file system info
        FSInfo fs_info;
        LittleFS.info(fs_info);

        Serial.println("Filesystem Info:");
        Serial.print("Total space:      ");
        Serial.print(fs_info.totalBytes);
        Serial.println(" bytes");

        Serial.print("Used space:       ");
        Serial.print(fs_info.usedBytes);
        Serial.println(" bytes");

        Serial.print("Free space:       ");
        Serial.print(fs_info.totalBytes - fs_info.usedBytes);
        Serial.println(" bytes");

        Serial.print("Block size:       ");
        Serial.print(fs_info.blockSize);
        Serial.println(" bytes");

        Serial.print("Page size:        ");
        Serial.print(fs_info.pageSize);
        Serial.println(" bytes");

        Serial.print("Max open files:   ");
        Serial.println(fs_info.maxOpenFiles);

        Serial.print("Max path length:  ");
        Serial.println(fs_info.maxPathLength);
    }
}

void initSettings()
{
    if (readSettings(settings, settingsFile))
    {
        Serial.println("Settings loaded");
        Serial.println("Device Name: " + settings.deviceName);
    }
    else
    {
        Serial.println("Failed to read settings");
        ESP.restart();
    }
}

void initAHT10Sensor()
{
    while (!AHT10.begin())
    {
        Serial.println("Can't find AHT10...");
        ledOn();
        delay(500);
        ledOff();
        delay(500);
    }
    Serial.println("AHT10 found");
}

void initBH1750Sensor()
{
    while (!lightMeter.begin(BH1750::CONTINUOUS_LOW_RES_MODE))
    {
        Serial.println(F("Can't find BH1750..."));
        ledOn();
        delay(500);
        ledOff();
        delay(500);
    }
    Serial.println("BH1750 found");
}

void handleNodeState()
{
    unsigned long startTime = millis();
    bool masterDetected = false;
    while ((millis() - startTime) < settings.masterListenDuration)
    {
        int packetSize = udp.parsePacket();
        if (packetSize)
        {
            char incomingPacket[255];
            int len = udp.read(incomingPacket, 255);
            if (len > 0)
            {
                incomingPacket[len] = '\0';
            }
            Serial.printf("[LM] Received packet: %s\n", incomingPacket);
            if (strncmp(incomingPacket, "IOT-LED_MASTER_IP", strlen("IOT-LED_MASTER_IP")) == 0)
            {
                // get IP address of the master node from udp message
                masterIP = udp.remoteIP();
                Serial.println("[LM] Master node detected. [" + masterIP.toString() + "]");
                // Send registration http request to master node
                if (WiFi.status() == WL_CONNECTED)
                {
                    WiFiClient client;
                    HTTPClient http;
                    String masterIP = udp.remoteIP().toString();
                    String registrationURL = "http://" + masterIP + "/register?ip=" + localIP.toString() + "&mac=" + WiFi.macAddress() + "&name=" + settings.deviceName;
                    Serial.print("Sending registration request to master node: ");
                    Serial.println(registrationURL);
                    http.begin(client, registrationURL);
                    int httpResponseCode = http.GET();
                    if (httpResponseCode == 200)
                    {
                        Serial.println("Registration successful");
                        masterDetected = true;
                        break;
                    }
                    else
                    {
                        Serial.print("Registration failed. Error code: ");
                        Serial.println(httpResponseCode);
                        ESP.restart();
                    }
                    http.end();
                }
                else
                {
                    Serial.println("Not connected to Wi-Fi. Registration failed.");
                    ESP.restart();
                }
            }
        }

        delay(500);
        ledOn();
        Serial.print(".");
        ledOff();
    }
    Serial.println();
    // If no master node detected, become the master node
    if (!masterDetected)
    {
        isMaster = true;
        masterIP = WiFi.localIP();
        Serial.println("No master node detected. Becoming master node.");
        Device masterDevice;
        masterDevice.ip = masterIP.toString();
        masterDevice.mac = WiFi.macAddress();
        masterDevice.name = settings.deviceName;
        masterDevice.isMaster = true;
        devices.push_back(masterDevice);
    }
    ledOff();
}

void setup()
{
    Serial.begin(9600);
    pinMode(MOSFET_LED_PIN, OUTPUT);
    pinMode(PIR_PIN, INPUT);
    pinMode(BATTERY_PIN, INPUT);
    pinMode(MOSFET_LIGHT_MODULE_PIN, OUTPUT);
    randomSeed(analogRead(A0));
    initLED();
    Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
    delay(100);
    initLittleFS();
    initSettings();
    connectToWiFi();
    setupOTA();
    initAHT10Sensor();
    initBH1750Sensor();
    udp.begin(settings.broadcastPort);
    setupRoutes();
    server.begin();
    timeClient.begin();
    delay(random(10, 300)); // if a few nodes will turn on in same time
    handleNodeState();
}

void handleMasterBroadcast(unsigned long currentTime)
{
    if (isMaster)
    {
        if (currentTime - lastBroadcastMsgTime >= settings.broadcastInterval || lastBroadcastMsgTime == 0)
        {
            ledOn();
            lastBroadcastMsgTime = currentTime;
            udp.beginPacket(broadcastIP, settings.broadcastPort);
            udp.write("IOT-LED_MASTER_IP");
            udp.endPacket();
            Serial.print("Broadcasting as master node. ");
            Serial.println(("IOT-LED_MASTER_IP=" + localIP.toString()).c_str());
            ledOff();
        }
    }
}

void handleNodeBroadcast(unsigned long currentTime)
{
    if (!isMaster)
    {
        int packetSize = udp.parsePacket();
        if (packetSize)
        {
            ledOn();
            char incomingPacket[255];
            int len = udp.read(incomingPacket, 255);
            if (len > 0)
            {
                incomingPacket[len] = '\0';
            }
            Serial.printf("Received packet: %s\n", incomingPacket);
            if (strncmp(incomingPacket, "IOT-LED_MASTER_IP", strlen("IOT-LED_MASTER_IP")) == 0)
            {
                lastMasterUpdateTime = currentTime;
            }
            ledOff();
        }
        if (currentTime - lastMasterUpdateTime >= settings.broadcastInterval * 3)
        {
            Serial.println("Master node not detected. Restarting...");
            ESP.restart();
        }
    }
}

void handleLightModuleActivity(unsigned long currentTime)
{
    if (isLightOffByUserConmmand || isLightOnByUserConmmand) {
        return;
    }
    if (currentTime - lastLightLevelCheck >= settings.checkLightingLevelInterval || lastLightLevelCheck == 0)
    {
        cLight = lightMeter.readLightLevel();
        Serial.println("Light: " + String(cLight) + " Lx");
        if (cLight < settings.minLightLevel)
        {
            if (!isBatteryLow && !isLightModuleActive)
            {
                Serial.println("Light not enough, turn on light module.");
                digitalWrite(MOSFET_LIGHT_MODULE_PIN, HIGH);
                isLightModuleActive = true;
            }
        }
        else
        {
            if (!isLightOn && cLight > settings.minLightLevel)
            {
                Serial.println("Light enough, turn off light module.");
                digitalWrite(MOSFET_LIGHT_MODULE_PIN, LOW);
                isLightModuleActive = false;
            }
        }
        lastLightLevelCheck = currentTime;
    }
}

void handleLightCycle(unsigned long currentTime)
{
    if (isLightOffByUserConmmand || isLightOnByUserConmmand) {
        return;
    }
    if (isLightModuleActive)
    {
        if (currentTime - lastMotionCheckTime >= settings.pirSensorCheckInterval || lastMotionCheckTime == 0)
        {
            int pirState = digitalRead(PIR_PIN);
            if (pirState == HIGH)
            {
                lastMoveTime = currentTime;
                if (prevPirState == LOW)
                {
                    Serial.println("Motion detected, light on");
                    digitalWrite(MOSFET_LED_PIN, HIGH);
                    isLightOn = true;
                }
            }
            if (isLightOn && currentTime - lastMoveTime >= settings.lightDuration)
            {
                digitalWrite(MOSFET_LED_PIN, LOW);
                Serial.println("Timer runed out. Light off");
                isLightOn = false;
            }
            prevPirState = pirState;
        }
    }
}

void handleBatteryLevel(unsigned long currentTime)
{
    if (currentTime - lastBatteryCheckTime >= settings.checkVBatInterval || lastBatteryCheckTime == 0)
    {
        int raw = analogRead(BATTERY_PIN);
        float voltageOnA0 = (raw / settings.maxADC) * settings.refVoltage;
        Serial.println("Voltage on A0: " + String(voltageOnA0));
        float vbat = voltageOnA0 * (settings.vdR1 + settings.vdR2) / settings.vdR2;
        VoltageLogger::addNewPoint(vbat);
        cvBat = vbat;
        Serial.println("Battery voltage: " + String(vbat));
        if (vbat < settings.vbatMin)
        {
            isBatteryLow = true;
            Serial.println("Battery low. Turning off light.");
            digitalWrite(MOSFET_LED_PIN, LOW);
            isLightOn = false;
            digitalWrite(MOSFET_LIGHT_MODULE_PIN, LOW);
            isLightModuleActive = false;
            ESP.deepSleep(1000);
        }
        else
        {
            isBatteryLow = false;
        }
        lastBatteryCheckTime = currentTime;
    }
}

void handleEnvironmentCheck(unsigned long currentTime)
{
    if (currentTime - lastAHTUpdate >= settings.checkEnvironmentInterval || lastAHTUpdate == 0)
    {
        sensors_event_t humidity, temp;
        AHT10.getEvent(&humidity, &temp);
        HumidityLogger::addNewPoint(humidity.relative_humidity);
        Serial.println("Humidity: " + String(humidity.relative_humidity) + " %");
        cHumidity = humidity.relative_humidity;
        TemperatureLogger::addNewPoint(temp.temperature);
        cTemperature = temp.temperature;
        Serial.println("Temperature: " + String(temp.temperature) + " C");
        lastAHTUpdate = currentTime;
    }
}

void loop()
{
    unsigned long currentTime = millis();
    timeClient.update();
    ArduinoOTA.handle();
    handleLightModuleActivity(currentTime);
    handleMasterBroadcast(currentTime);
    handleNodeBroadcast(currentTime);
    handleLightCycle(currentTime);
    handleEnvironmentCheck(currentTime);
    handleBatteryLevel(currentTime);
}