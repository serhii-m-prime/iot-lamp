#include <Arduino.h>
#include <ArduinoOTA.h>
#include <LittleFS.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <WiFiClient.h>
#include <WiFiUdp.h>
#include <NTPClient.h>
#include <ESP8266HTTPClient.h>
#include "led.h"
#include "settings.h"
#include <Adafruit_AHTX0.h>
#include <Wire.h>

// #define MOSFET_PIR_PIN 7
#define MOSFET_LED_PIN 5 // D1
#define PIR_PIN 4        // D2
#define BATTERY_PIN A0



struct Device
{
    String ip;
    String mac;
    String name;
};

const char *settingsFile = "/settings.txt";

bool isMaster = false;
bool isLightOn = false;
bool isBatteryLow = false;
int prevPirState = LOW;
unsigned long lastBroadcastMsgTime = 0;
unsigned long lastMasterUpdateTime = 0;
unsigned long lastMoveTime = 0;
unsigned long lastAHTUpdate = 0;
std::vector<Device> devices;
Settings settings;

Adafruit_AHTX0 AHT10;

WiFiUDP udp;
IPAddress broadcastIP;
IPAddress localIP;
IPAddress masterIP;
ESP8266WebServer server(80);
WiFiUDP ntcudp;
NTPClient timeClient(ntcudp, "europe.pool.ntp.org");

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

void connectToWiFi(const Settings &settings)
{
    Serial.print("Connecting to Wi-Fi: ");
    Serial.println(settings.wifi_ssid);

    WiFi.begin(settings.wifi_ssid.c_str(), settings.wifi_password.c_str());

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

void handleStatus()
{
    String status = "isMater=" + String(isMaster) + "\n";
    status += "Name: " + settings.device_name + "\n";
    status += "IP: " + localIP.toString() + "\n";
    status += "MAC: " + WiFi.macAddress() + "\n";
    status += "LightSattaus: " + String(isLightOn) + "\n";
    status += "BatteryLow: " + String(isBatteryLow) + "\n";
    status += "Battary voltage: \n";
    status += "Humidity: \n";
    status += "Temperature: \n";
    status += "Time: " + timeClient.getFormattedTime() + "\n";

    server.send(200, "text/plain", status);
}

void handleRegister()
{
    Device newDevice;
    newDevice.ip = server.arg("ip");
    newDevice.mac = server.arg("mac");
    newDevice.name = server.arg("name");
    // check if device already registered
    for (const auto &device : devices)
    {
        if (device.mac == newDevice.mac)
        {
            server.send(200, "text/plain", "Device already registered");
            return;
        }
    }
    devices.push_back(newDevice);
    server.send(200, "text/plain", "Registration successful");
    Serial.println("Devices:");
    for (const auto &device : devices)
    {
        Serial.print("IP: ");
        Serial.println(device.ip);
        Serial.print("MAC: ");
        Serial.println(device.mac);
        Serial.print("Name: ");
        Serial.println(device.name);
        Serial.println();
    }
}

void setup()
{
    Serial.begin(9600);
    pinMode(MOSFET_LED_PIN, OUTPUT);
    // pinMode(MOSFET_PIR_PIN, OUTPUT);
    pinMode(PIR_PIN, INPUT);
    pinMode(BATTERY_PIN, INPUT);
    initLED();
    Wire.begin(D5, D6);
    delay(333);
    if (LittleFS.begin())
    {
        listFiles();
    }
    else
    {
        Serial.println("Failed to mount LittleFS.");
    }
    randomSeed(analogRead(A0));
    if (readSettings(settings, settingsFile))
    {
        Serial.println("Settings loaded");
        Serial.print("Device Name: ");
        Serial.println(settings.device_name);
        connectToWiFi(settings);
    }
    else
    {
        Serial.println("Failed to read settings");
    }

    // Setup OTA
    ArduinoOTA.setHostname("esp8266-ota");

    ArduinoOTA.onStart([]() {
        String type;
        if (ArduinoOTA.getCommand() == U_FLASH) {
            type = "sketch";
        } else {
            type = "filesystem";
        }
        Serial.println("Start updating " + type);
    });

    ArduinoOTA.onEnd([]() {
        Serial.println("\nEnd");
    });

    ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
        Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
    });

    ArduinoOTA.onError([](ota_error_t error) {
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
        }
    });

    ArduinoOTA.begin();

    localIP = WiFi.localIP();
    IPAddress subnetMask = WiFi.subnetMask();
    broadcastIP = calculateBroadcastAddress(localIP, subnetMask);
    Serial.print("Broadcast IP address: ");
    Serial.println(broadcastIP);
    udp.begin(settings.broadcast_port);

    server.on("/status", handleStatus);
    server.on("/register", handleRegister);
    server.begin();

    while (!AHT10.begin())
    {
        Serial.println("Could not find AHT? Check wiring");
        ledOn();
        delay(500);
        ledOff();
        delay(500);
    }
    Serial.println("AHT10 found");
    timeClient.begin();
    // Waiting master node
    unsigned long startTime = millis();
    delay(random(10, 300));
    while (millis() - startTime < settings.master_listen_duration)
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
                    String registrationURL = "http://" + masterIP + "/register?ip=" + localIP.toString() + "&mac=" + WiFi.macAddress() + "&name=" + settings.device_name;
                    Serial.print("Sending registration request to master node: ");
                    Serial.println(registrationURL);

                    http.begin(client, registrationURL);
                    int httpResponseCode = http.GET();
                    if (httpResponseCode == 200)
                    {
                        Serial.println("Registration successful");
                    }
                    else
                    {
                        Serial.print("Registration failed. Error code: ");
                        Serial.println(httpResponseCode);
                    }
                    http.end();
                }
                else
                {
                    Serial.println("Not connected to Wi-Fi. Registration failed.");
                }
                return; // Exit setup if master node is detected
            }
        }
        delay(200);
    }

    // If no master node detected, become the master node
    isMaster = true;
    Serial.println("No master node detected. Becoming master node.");
}

void handleMasterBroadcast(unsigned long currentTime)
{
    if (isMaster)
    {
        if (currentTime - lastBroadcastMsgTime >= settings.broadcast_interval)
        {
            ledOn();
            lastBroadcastMsgTime = currentTime;
            udp.beginPacket(broadcastIP, settings.broadcast_port);
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
        }
        if (currentTime - lastMasterUpdateTime >= settings.broadcast_interval * 3)
        {
            Serial.println("Master node not detected. Restarting...");
            ESP.restart();
        }
    }
}

void handleLightCycle(unsigned long currentTime)
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
    if (isLightOn && currentTime - lastMoveTime >= settings.light_duration)
    {
        digitalWrite(MOSFET_LED_PIN, LOW);
        Serial.println("Timer runed out. Light off");
        isLightOn = false;
    }
    prevPirState = pirState;
}

float readBatteryVoltage()
{
    int raw = analogRead(BATTERY_PIN);
    float voltageOnA0 = (raw / settings.maxADC) * settings.refVoltage;
    Serial.print("Voltage on A0: ");
    Serial.println(voltageOnA0);
    float vbat = voltageOnA0 * (settings.R1 + settings.R2) / settings.R2;
    return vbat;
}

void loop()
{
    unsigned long currentTime = millis();
    timeClient.update();
    ArduinoOTA.handle();
    handleMasterBroadcast(currentTime);
    handleNodeBroadcast(currentTime);
    handleLightCycle(currentTime);
    server.handleClient();
    if (currentTime - lastAHTUpdate >= 10000)
    {
        sensors_event_t humidity, temp;
        AHT10.getEvent(&humidity, &temp);
        Serial.print("Humidity: ");
        Serial.print(humidity.relative_humidity);
        Serial.println(" %");
        Serial.print("Temperature: ");
        Serial.print(temp.temperature);
        Serial.println(" C");
        lastAHTUpdate = currentTime;


        Serial.println("Battery voltage: " + String(readBatteryVoltage()) + " V");
        Serial.println(timeClient.getEpochTime());
    }
    // iteration time 
    Serial.println("Iteration time: " + String(millis() - currentTime) + " ms");
    delay(930);
}