#include "WebHandler.h"
#include "settings.h"
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include <NTPClient.h>
#include <LittleFS.h>
#include "HumidityLogger.h"
#include "TemperatureLogger.h"
#include "VoltageLogger.h"

extern const char *settingsFile;
extern AsyncWebServer server;
extern Settings settings;
extern bool isMaster;
extern std::vector<Device> devices;
extern IPAddress localIP;
extern IPAddress masterIP;
extern bool isLightModuleActive;
extern bool isLightOn;
extern bool isBatteryLow;
extern NTPClient timeClient;
extern float cHumidity;
extern float cTemperature;
extern float cvBat;
extern float cLight;
extern bool isLightOffByUserConmmand;
extern bool isLightOnByUserConmmand;

#define MOSFET_LIGHT_MODULE_PIN 5 // D8
#define MOSFET_LED_PIN 13 // D1

String logedSession;
unsigned long sessionExpiteAt;
const String sessionCookie = "ESPSESSID";

bool isUserLogedIn(AsyncWebServerRequest *request)
{
    String cookieHeader = request->header("Cookie");
    String cookieValue = "";

    // Extract specific cookie value
    int startIndex = cookieHeader.indexOf(sessionCookie + "=");
    if (startIndex >= 0)
    {
        startIndex += 10; // Skip "myCookie="
        int endIndex = cookieHeader.indexOf(';', startIndex);
        if (endIndex == -1)
            endIndex = cookieHeader.length();
        cookieValue = cookieHeader.substring(startIndex, endIndex);
    }
    if (cookieValue == logedSession && sessionExpiteAt > timeClient.getEpochTime())
    {
        return true;
    }
    else
    {
        request->send(401, "text/plain", "Unauthorized");
        return false;
    }
}

void handleLogin(AsyncWebServerRequest *request)
{
    if (request->method() == HTTP_GET)
    {
        char uploadPage[] = R"(
            <h1>LOGIN</h1>
        <form method="POST" action="/login" enctype="multipart/form-data">
        <lable for="name">Name:</lable>
            <input type="text" name="name" /><br>
            <lable for="pwd">Password:</lable>
            <input type="text" name="pwd" /><br>
            <input type="submit" value="Log-in" />
        </form>
        )";
        request->send(200, "text/html", uploadPage);
    }
    else if (request->method() == HTTP_POST)
    {
        if (request->hasParam("pwd", true) && request->getParam("pwd", true)->value() == settings.usrPassword && request->hasParam("name", true) && request->getParam("name", true)->value() == settings.usrName)
        {
            int length = 17;
            String characters = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
            String randomString = "";
            for (int i = 0; i < length; i++)
            {
                int index = random(characters.length());
                randomString += characters[index];
            }
            logedSession = randomString;
            sessionExpiteAt = timeClient.getEpochTime() + 1000 * 60 * 60;
            AsyncWebServerResponse *response = request->beginResponse(302, "text/html", "<a href='/'>Home</a><br><a href='/settings'>Settings</a>");
            response->addHeader("Set-Cookie", sessionCookie + "=" + randomString + "; Path=/; HttpOnly; Max-Age=3600");
            request->send(response);
        }
        else
        {
            request->send(401, "text/plain", "Unauthorized");
        }
    }
}

void handleLogout(AsyncWebServerRequest *request)
{
    if (isUserLogedIn(request))
    {
        logedSession = "";
        sessionExpiteAt = 0;
        request->send(200, "text/plain", "Logout successful");
    }
}

// 404 Not Found handler
void handleNotFound(AsyncWebServerRequest *request)
{
    request->send(404, "text/html", "<h1>404 - Page Not Found</h1>");
}

void handleStatus(AsyncWebServerRequest *request)
{
    JsonDocument doc;
    doc["isMaster"] = isMaster;
    doc["masterIP"] = masterIP.toString();
    doc["name"] = settings.deviceName;
    doc["ip"] = localIP.toString();
    doc["mac"] = WiFi.macAddress();
    doc["LightModuleActive"] = isLightModuleActive;
    doc["lightStatus"] = isLightOn;
    doc["lightOffByUserCommand"] = isLightOffByUserConmmand;
    doc["lightOnByUserCommand"] = isLightOnByUserConmmand;
    doc["lightDuration"] = settings.lightDuration;
    doc["batteryLow"] = isBatteryLow;
    doc["batteryVoltage"] = cvBat;
    doc["minBatteryVoltage"] = settings.vbatMin;
    doc["humidity"] = cHumidity;
    doc["temperature"] = cTemperature;
    doc["lightLevel"] = cLight;
    doc["minLightLevel"] = settings.minLightLevel;
    doc["time"] = timeClient.getEpochTime();
    String status;
    serializeJson(doc, status);
    request->send(200, "application/json", status);
}

void handleRegister(AsyncWebServerRequest *request)
{
    Device newDevice;
    if (request->hasParam("ip"))
    {
        newDevice.ip = request->getParam("ip")->value();
    }
    else
    {
        request->send(400, "text/plain", "Missing IP parameter");
        return;
    }
    if (request->hasParam("mac"))
    {
        newDevice.mac = request->getParam("mac")->value();
    }
    else
    {
        request->send(400, "text/plain", "Missing MAC parameter");
        return;
    }
    if (request->hasParam("name"))
    {
        newDevice.name = request->getParam("name")->value();
    }
    else
    {
        request->send(400, "text/plain", "Missing name parameter");
        return;
    }
    newDevice.isMaster = false;
    // check if device already registered
    for (const auto &device : devices)
    {
        if (device.mac == newDevice.mac)
        {
            request->send(200, "text/plain", "Device already registered");
            return;
        }
    }
    devices.push_back(newDevice);
    request->send(200, "text/plain", "Registration successful");
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

void handleIndex(AsyncWebServerRequest *request)
{
    if (isMaster)
    {
        AsyncWebServerResponse *response = request->beginResponse(LittleFS, "/index.html", "text/html");
        request->send(response);
    }
    else
    {
        AsyncWebServerResponse *response = request->beginResponse(302, "text/plain", "Redirecting to master node...");
        response->addHeader("Location", "http://" + masterIP.toString() + "/");
        request->send(response);
    }
}

void handleSats(AsyncWebServerRequest *request)
{
    AsyncWebServerResponse *response = request->beginResponse(LittleFS, "/stats.html", "text/html");
    request->send(response);
}

void handleDevicesList(AsyncWebServerRequest *request)
{
    JsonDocument doc;
    JsonArray devicesArray = doc["devices"].to<JsonArray>();
    for (const auto &device : devices)
    {
        JsonObject deviceObject = devicesArray.createNestedObject();
        deviceObject["ip"] = device.ip;
        deviceObject["mac"] = device.mac;
        deviceObject["name"] = device.name;
        deviceObject["isMaster"] = device.isMaster;
    }
    String response;
    serializeJson(doc, response);
    request->send(200, "application/json", response);
}

void renderSettingsPage(AsyncWebServerRequest *request)
{
    if (isUserLogedIn(request))
    {

        char uploadPage[] = R"(
    <!DOCTYPE html>
    <html>
    <head>
    <title>Settings</title>
    </head>
    <body>
    <a href="/">Home</a><br>
    <a href="/logout">Logout</a><br>
    <h1>Settings</h1>
    <pre>%s</pre>
  <form method="POST" action="/uploadSettings" enctype="multipart/form-data">
    <input type="file" name="file" />
    <input type="submit" value="Upload File" />
  </form>
    </body>
    </html>
)";
        char pageBuffer[1024];
        snprintf(pageBuffer, sizeof(pageBuffer), uploadPage, settings.toString().c_str());
        request->send(200, "text/html", pageBuffer);
    }
}

void handleUploadSettings(AsyncWebServerRequest *request)
{

    if (isUserLogedIn(request))
    {
        request->send(200, "text/plain", "File Uploaded Successfully. Restarting");
        return;
    }
}

void handleUploadSettingsCb(AsyncWebServerRequest *request, const String &filename, size_t index, uint8_t *data, size_t len, bool final)
{
    if (isUserLogedIn(request))
    {
        if (!index)
        {
            Serial.printf("Uploading file: %s\n", filename.c_str());
            if (LittleFS.exists(settingsFile))
            {
                LittleFS.remove(settingsFile); // Delete the old file
            }
            // Open the file for writing in binary mode
            request->_tempFile = LittleFS.open(settingsFile, "w");
        }
        if (len)
        {
            // Write the data to the file
            request->_tempFile.write(data, len);
        }
        if (final)
        {
            // Close the file once upload is finished
            request->_tempFile.close();
            request->onDisconnect([]()
                                  {
            delay(1000L);
            Serial.println("Got new setings. Restarting...");
            ESP.restart(); });
        }
    }
}

void rturnTemperatureDataSet(AsyncWebServerRequest *request)
{
    request->send(200, "application/json", TemperatureLogger::getAllPoints());
}

void returnHumidityDataSet(AsyncWebServerRequest *request)
{
    request->send(200, "application/json", HumidityLogger::getAllPoints());
}

void returnVBatDataSet(AsyncWebServerRequest *request)
{
    request->send(200, "application/json", VoltageLogger::getAllPoints());
}


void handleLightOn(AsyncWebServerRequest *request)
{
    digitalWrite(MOSFET_LIGHT_MODULE_PIN, HIGH);
    isLightOffByUserConmmand = false;
    isLightOnByUserConmmand = true;
    digitalWrite(MOSFET_LED_PIN, HIGH);
    isLightOn = true;
    digitalWrite(MOSFET_LIGHT_MODULE_PIN, HIGH);
    isLightModuleActive = true;
    Serial.println("Light module turned on by user command.");
    request->send(200, "application/json", "{\"status\": true}");
}

void handleLightOff(AsyncWebServerRequest *request)
{
    digitalWrite(MOSFET_LIGHT_MODULE_PIN, LOW);
    isLightOffByUserConmmand = true;
    isLightOnByUserConmmand = false;
    digitalWrite(MOSFET_LED_PIN, LOW);
    isLightOn = false;
    digitalWrite(MOSFET_LIGHT_MODULE_PIN, LOW);
    isLightModuleActive = false;
    Serial.println("Light module turned off by user command.");
    request->send(200, "application/json", "{\"status\": true}");
}

void handleLightAuto(AsyncWebServerRequest *request)
{
    isLightOffByUserConmmand = false;
    isLightOnByUserConmmand = false;
    digitalWrite(MOSFET_LIGHT_MODULE_PIN, LOW);
    isLightModuleActive = false;
    digitalWrite(MOSFET_LED_PIN, LOW);
    isLightOn = false;
    Serial.println("Auto light mode activated.");
    request->send(200, "application/json", "{\"status\": true}");
}

void setupRoutes()
{
    server.serveStatic("/", LittleFS, "/").setDefaultFile("index.html");
    server.onNotFound(handleNotFound);
    server.on("/login", handleLogin);
    server.on("/logout", handleLogout);
    server.on("/status", handleStatus);
    server.on("/register", handleRegister);
    server.on("/", handleIndex);
    server.on("/devices", handleDevicesList);
    server.on("/stats", handleSats);
    server.on("/settings", renderSettingsPage);
    server.on("/uploadSettings", HTTP_POST, handleUploadSettings, handleUploadSettingsCb);
    server.on("/temperature", rturnTemperatureDataSet);
    server.on("/humidity", returnHumidityDataSet);
    server.on("/vbat", returnVBatDataSet);
    server.on("/lightOn", handleLightOn);
    server.on("/lightOff", handleLightOff);
    server.on("/lightAuto", handleLightAuto);
}