#include <Arduino.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>
#include <ESPmDNS.h>
#include "SPIFFS.h"
#include <FS.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include "sd_read_write.h"
#include "SD_MMC.h"
#include "time.h"
#include <ArduinoJson.h>

const char *ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 3600;
const int daylightOffset_sec = 3600;

// Pin definitions for SD card
#define SD_MMC_CMD 38 // Please do not modify it.
#define SD_MMC_CLK 39 // Please do not modify it.
#define SD_MMC_D0 40  // Please do not modify it.

// Data logging
unsigned long lastReadingTime = 0;
unsigned long lastAverageTime = 0;
const unsigned long readingInterval = 5000;  // 5 seconds in milliseconds
const unsigned long averageInterval = 30000; // 30 seconds in milliseconds
float averageTemp = 0.0;
int iterations = 1;

// Create AsyncWebServer object on port 80
AsyncWebServer server(80);

// Search for parameter in HTTP POST request
const char *PARAM_INPUT_1 = "ssid";
const char *PARAM_INPUT_2 = "pass";
const char *PARAM_INPUT_3 = "ip";
const char *PARAM_INPUT_4 = "gateway";

// Variables to save values from HTML form
String ssid;
String pass;
String ip;
String gateway;

// File paths to save input values permanently
const char *ssidPath = "/ssid.txt";
const char *passPath = "/pass.txt";
const char *ipPath = "/ip.txt";
const char *gatewayPath = "/gateway.txt";

IPAddress localIP;
// IPAddress localIP(192, 168, 1, 200); // hardcoded

// Set your Gateway IP address
IPAddress localGateway;
// IPAddress localGateway(192, 168, 1, 1); //hardcoded
IPAddress subnet(255, 255, 0, 0);
IPAddress dns(8, 8, 8, 8);

// Timer variables
unsigned long previousMillis = 0;
const long interval = 10000; // interval to wait for Wi-Fi connection (milliseconds)

// GPIO where the DS18B20 is connected to
const int oneWireBus = 4;

// Setup a oneWire instance to communicate with any OneWire devices
OneWire oneWire(oneWireBus);

// Pass our oneWire reference to Dallas Temperature sensor
DallasTemperature sensors(&oneWire);

// Prototypes
bool initWiFi();
void initSPIFFS();
void initSDCard();
String readFileFS(fs::FS &fs, const char *path);
void writeFileFS(fs::FS &fs, const char *path, const char *message);
String getLocalTime();
void writeFileSD(String data);
void readTemp();
String getSensorData();
void deleteNetworkSettings();

// Debugging prototypes
void readFileSDDEBUG();
void resetFileSDDEBUG();

void setup()
{
  // Serial port for debugging purposes
  Serial.begin(115200);

  // Start the DS18B20 sensor
  sensors.begin();

  initSPIFFS();

  // Load values saved in SPIFFS
  ssid = readFileFS(SPIFFS, ssidPath);
  pass = readFileFS(SPIFFS, passPath);
  ip = readFileFS(SPIFFS, ipPath);
  gateway = readFileFS(SPIFFS, gatewayPath);
  Serial.println(ssid);
  Serial.println(pass);
  Serial.println(ip);
  Serial.println(gateway);

  // Default values for debugging on school network
  // ssid = "E308";
  // pass = "98806829";
  // ip = "192.168.0.111";
  // gateway = "192.168.0.1";
  // Remember to delete above default values for production

  if (initWiFi())
  {
    // Route for root / web page
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request)
              { request->send(SPIFFS, "/index.html", "text/html", false); });
    server.serveStatic("/", SPIFFS, "/");

    // Sends JSON data to client
    server.on("/getData", HTTP_GET, [](AsyncWebServerRequest *request)
              {
  String jsonData = getSensorData();
  if (jsonData.isEmpty()) {
    request->send(500, "text/plain", "Error reading sensor data");
    return;
  }

  request->send(200, "application/json", jsonData); });

    // Delete network settings
    server.on("/deleteNetwork", HTTP_GET, [](AsyncWebServerRequest *request)
              {
      writeFileFS(SPIFFS, ssidPath, "");
      writeFileFS(SPIFFS, passPath, "");
      writeFileFS(SPIFFS, ipPath, "");
      writeFileFS(SPIFFS, gatewayPath, "");
      request->send(200, "text/plain", "Network settings deleted. ESP will restart.");
      delay(3000);
      ESP.restart(); });

    // Delete data log
    server.on("/deleteDataLog", HTTP_GET, [](AsyncWebServerRequest *request)
              {
      deleteFile(SD_MMC, "/data/datalog.csv");      
      
      if (SD_MMC.exists("/data/datalog.csv"))
      {
        Serial.println("datalog.csv exists.");
      }
      else
      {
        File dataFile = SD_MMC.open("/data/datalog.csv", FILE_WRITE);
        if (dataFile)
        {
          Serial.println("datalog.csv created.");
        }
        else
        {
          Serial.println("Error creating CSV file.");
        }
          dataFile.close();
      }
      
      

        request->send(200, "text/plain", "Data log deleted."); });

    server.begin();
  }
  else
  {
    // Connect to Wi-Fi network with SSID and password
    Serial.println("Setting AP (Access Point)");
    // NULL sets an open Access Point
    WiFi.softAP("DDEV-WIFI-MANAGER", NULL);

    IPAddress IP = WiFi.softAPIP();
    Serial.print("AP IP address: ");
    Serial.println(IP);

    // Web Server Root URL
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request)
              { request->send(SPIFFS, "/wifimanager.html", "text/html"); });

    server.serveStatic("/", SPIFFS, "/");

    server.on("/", HTTP_POST, [](AsyncWebServerRequest *request)
              {
      int params = request->params();
      for(int i=0;i<params;i++){
        AsyncWebParameter* p = request->getParam(i);
        if(p->isPost()){
          // HTTP POST ssid value
          if (p->name() == PARAM_INPUT_1) {
            ssid = p->value().c_str();
            Serial.print("SSID set to: ");
            Serial.println(ssid);
            // Write file to save value
            writeFileFS(SPIFFS, ssidPath, ssid.c_str());
          }
          // HTTP POST pass value
          if (p->name() == PARAM_INPUT_2) {
            pass = p->value().c_str();
            Serial.print("Password set to: ");
            Serial.println(pass);
            // Write file to save value
            writeFileFS(SPIFFS, passPath, pass.c_str());
          }
          // HTTP POST ip value
          if (p->name() == PARAM_INPUT_3) {
            ip = p->value().c_str();
            Serial.print("IP Address set to: ");
            Serial.println(ip);
            // Write file to save value
            writeFileFS(SPIFFS, ipPath, ip.c_str());
          }
          // HTTP POST gateway value
          if (p->name() == PARAM_INPUT_4) {
            gateway = p->value().c_str();
            Serial.print("Gateway set to: ");
            Serial.println(gateway);
            // Write file to save value
            writeFileFS(SPIFFS, gatewayPath, gateway.c_str());
          }
          //Serial.printf("POST[%s]: %s\n", p->name().c_str(), p->value().c_str());
        }
      }
      request->send(200, "text/plain", "Done. ESP will restart, connect to your router and go to IP address: " + ip);
      delay(3000);
      ESP.restart(); });
    server.begin();
  }

  // Init and get the time
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  Serial.println("Waiting for time");
  Serial.println(getLocalTime());

  initSDCard();

  // Debugging
  // resetFileSDDEBUG(); // Reset file for debugging
  readFileSDDEBUG(); // Read file for debugging

} // end setup

void loop()
{
  readTemp();
}

// Initialize SPIFFS
void initSPIFFS()
{
  if (!SPIFFS.begin(true))
  {
    Serial.println("An error has occurred while mounting SPIFFS");
  }
  Serial.println("SPIFFS mounted successfully");
}

// Initialize WiFi
bool initWiFi()
{
  if (ssid == "" || ip == "")
  {
    Serial.println("Undefined SSID or IP address.");
    return false;
  }

  WiFi.mode(WIFI_STA);
  localIP.fromString(ip.c_str());
  localGateway.fromString(gateway.c_str());

  if (!WiFi.config(localIP, localGateway, subnet, dns))
  {
    Serial.println("STA Failed to configure");
    return false;
  }
  WiFi.begin(ssid.c_str(), pass.c_str());
  Serial.println("Connecting to WiFi...");

  unsigned long currentMillis = millis();
  previousMillis = currentMillis;

  while (WiFi.status() != WL_CONNECTED)
  {
    currentMillis = millis();
    if (currentMillis - previousMillis >= interval)
    {
      Serial.println("Failed to connect.");
      return false;
    }
  }

  if (!MDNS.begin("ddev-esp32"))
  {
    Serial.println("Error setting up MDNS responder!");
    while (1)
    {
      delay(1000);
    }
  }

  Serial.print("Connected to ");
  Serial.println(WiFi.localIP());
  return true;
}

// Initialize SD card
void initSDCard()
{
  Serial.println("Initializing SD card...");

  SD_MMC.setPins(SD_MMC_CLK, SD_MMC_CMD, SD_MMC_D0);
  if (!SD_MMC.begin("/sdcard", true, true, SDMMC_FREQ_DEFAULT, 5))
  {
    Serial.println("Card Mount Failed");
    return;
  }

  // Check if file exists
  if (SD_MMC.exists("/data/datalog.csv"))
  {
    Serial.println("datalog.csv exists.");
  }
  else
  {
    File dataFile = SD_MMC.open("/data/datalog.csv", FILE_WRITE);
    if (dataFile)
    {
      Serial.println("datalog.csv created.");
    }
    else
    {
      Serial.println("Error creating CSV file.");
    }
    dataFile.close();
  }
}

// Read File from SPIFFS
String readFileFS(fs::FS &fs, const char *path)
{
  Serial.printf("Reading file: %s\r\n", path);

  File file = fs.open(path);
  if (!file || file.isDirectory())
  {
    Serial.println("- failed to open file for reading");
    return String();
  }

  String fileContent;
  while (file.available())
  {
    fileContent = file.readStringUntil('\n');
    break;
  }
  return fileContent;
}

// Write file to SPIFFS
void writeFileFS(fs::FS &fs, const char *path, const char *message)
{
  Serial.printf("Writing file: %s\r\n", path);

  File file = fs.open(path, FILE_WRITE);
  if (!file)
  {
    Serial.println("- failed to open file for writing");
    return;
  }
  if (file.print(message))
  {
    Serial.println("- file written");
  }
  else
  {
    Serial.println("- frite failed");
  }
}

// Get local time
String getLocalTime()
{
  struct tm timeinfo;
  while (!getLocalTime(&timeinfo))
  {
    vTaskDelay(500);
    Serial.print(".");
  }

  char timeStringBuff[50];
  strftime(timeStringBuff, sizeof(timeStringBuff), "%Y-%m-%d %H:%M:%S", &timeinfo);
  return String(timeStringBuff); // Convert C-style string to String object
}

// Write data to SD card
void writeFileSD(String data)
{
  // Open file for writing
  File file = SD_MMC.open("/data/datalog.csv", FILE_APPEND);
  if (!file)
  {
    Serial.println("Failed to open file for writing");
  }
  if (file.print(data))
  {
    Serial.println("Data written to file");
  }
  else
  {
    Serial.println("Write failed");
  }
  file.close();
}

// Read Temperatur and write average to SD card
void readTemp()
{
  unsigned long currentTime = millis();

  // Check for temperature reading interval
  if (currentTime - lastReadingTime >= readingInterval)
  {
    lastReadingTime = currentTime;

    sensors.requestTemperatures(); // Request temperature reading
    float currentTemp = sensors.getTempCByIndex(0);

    averageTemp = (averageTemp * (iterations - 1) + currentTemp) / iterations;
    iterations++;

    Serial.print("Current Temp: ");
    Serial.print(currentTemp);
    Serial.println(" C - " + getLocalTime() + "\n");
  }

  // Check for average temperature update interval
  if (currentTime - lastAverageTime >= averageInterval)
  {
    lastAverageTime = currentTime;

    Serial.print("Average Temp: ");
    // Serial.print(averageTemp);
    // Serial.println(" C - " + getLocalTime() + " Past 30 seconds");
    String stringToSD = String(averageTemp) + "," + getLocalTime() + "\n";
    Serial.println(String(averageTemp));
    Serial.println(stringToSD);
    writeFileSD(stringToSD);
  }
}

String getSensorData()
{
  File file = SD_MMC.open("/data/datalog.csv");
  if (!file || file.isDirectory())
  {
    Serial.println("Failed to open file for reading");
    return ""; // Or return an error JSON if preferred
  }

  JsonDocument doc; // Adjust document size as needed
  JsonArray dataArray = doc["data"].to<JsonArray>();

  String line;
  while ((line = file.readStringUntil('\n')) != "")
  {
    // Split line into data points (on comma separation) store as string temp and date
    int commaIndex = line.indexOf(',');
    // split after comma until /n
    String tempStr = line.substring(0, commaIndex);
    String dateStr = line.substring(commaIndex + 1, line.length() );

    // Add data to JSON array
    JsonObject dataObj = dataArray.add<JsonObject>();

    dataObj["temperature"] = tempStr.toFloat();
    dataObj["date"] = dateStr;
  }

  file.close();

  String jsonString;
  serializeJson(doc, jsonString);
  return jsonString;
}

// Delete network settings
void deleteNetworkSettings()
{
  SPIFFS.remove(ssidPath);
  SPIFFS.remove(passPath);
  SPIFFS.remove(ipPath);
  SPIFFS.remove(gatewayPath);
}

// Below this line is for debugging purposes only

// Read file from SD card ONLY FOR DEBUGGING
void readFileSDDEBUG()
{
  File file = SD_MMC.open("/data/datalog.csv");
  if (!file || file.isDirectory())
  {
    Serial.println("Failed to open file for reading");
    return;
  }

  Serial.println("Reading from file:");
  while (file.available())
  {
    Serial.write(file.read());
  }
  file.close();
}

// Reset file from SD card ONLY FOR DEBUGGING
void resetFileSDDEBUG()
{
  deleteFile(SD_MMC, "/data/datalog.csv");

  writeFile(SD_MMC, "/data/datalog.csv", "");
}
