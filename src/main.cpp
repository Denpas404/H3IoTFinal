/**
 * @file main.cpp
 * @brief Main file for the ESP32 data logger project.
 * @details This file contains the main code for the ESP32 data logger project.
 * It includes necessary libraries, defines variables, and declares objects for various components.
 */

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

// time.h library variables
const char *ntpServer = "pool.ntp.org"; /**< NTP server address */
const long gmtOffset_sec = 3600;        /**< GMT offset in seconds */
const int daylightOffset_sec = 3600;    /**< Daylight offset in seconds */

// Pin definitions for SD card
#define SD_MMC_CMD 38 /**< SD card command pin */
#define SD_MMC_CLK 39 /**< SD card clock pin */
#define SD_MMC_D0 40  /**< SD card data pin */

// Data logging
unsigned long lastReadingTime = 0;       /**< Time of the last temperature reading */
unsigned long lastAverageTime = 0;       /**< Time of the last average temperature calculation */
const unsigned long readingInterval = 5000;  /**< Interval between temperature readings (5 seconds) */
const unsigned long averageInterval = 30000; /**< Interval between average temperature calculations (30 seconds) */
float averageTemp = 0.0;                 /**< Average temperature */
int iterations = 1;                      /**< Iterations counter */

// Create AsyncWebServer object on port 80
AsyncWebServer server(80);               /**< Web server object */

// Search for parameter in HTTP POST request
const char *PARAM_INPUT_1 = "ssid";      /**< Parameter name for SSID */
const char *PARAM_INPUT_2 = "pass";      /**< Parameter name for password */
const char *PARAM_INPUT_3 = "ip";        /**< Parameter name for IP address */
const char *PARAM_INPUT_4 = "gateway";   /**< Parameter name for gateway */

// Variables to save values from HTML form
String ssid;                              /**< SSID */
String pass;                              /**< Password */
String ip;                                /**< IP address */
String gateway;                           /**< Gateway */

// File paths to save input values permanently
const char *ssidPath = "/ssid.txt";       /**< SSID file path */
const char *passPath = "/pass.txt";       /**< Password file path */
const char *ipPath = "/ip.txt";           /**< IP address file path */
const char *gatewayPath = "/gateway.txt"; /**< Gateway file path */

IPAddress localIP;                       /**< Local IP address */
IPAddress localGateway;                  /**< Local gateway IP address */
IPAddress subnet(255, 255, 0, 0);        /**< Subnet mask */
IPAddress dns(8, 8, 8, 8);               /**< DNS server */

// Timer variables
unsigned long previousMillis = 0;        /**< Previous millis */
const long interval = 10000;             /**< Interval to wait for Wi-Fi connection (10 seconds) */

// GPIO where the DS18B20 is connected to
const int oneWireBus = 4;                /**< GPIO pin for DS18B20 */

// Setup a oneWire instance to communicate with any OneWire devices
OneWire oneWire(oneWireBus);             /**< OneWire instance */

// Pass our oneWire reference to Dallas Temperature sensor
DallasTemperature sensors(&oneWire);     /**< DallasTemperature sensor instance */
  

/**
 * @brief Function prototypes for initialization and debugging.
 * 
 * This section contains function prototypes for initializing various components and performing debugging tasks.
 */

/**
 * @brief Initialize Wi-Fi connection.
 * @return true if Wi-Fi connection is successful, false otherwise.
 */
bool initWiFi();

/**
 * @brief Initialize SPIFFS (SPI Flash File System).
 */
void initSPIFFS();

/**
 * @brief Initialize SD card.
 */
void initSDCard();

/**
 * @brief Read file from the file system.
 * 
 * @param fs The file system to read from.
 * @param path The path of the file to read.
 * @return String containing the file content.
 */
String readFileFS(fs::FS &fs, const char *path);

/**
 * @brief Write message to file in the file system.
 * 
 * @param fs The file system to write to.
 * @param path The path of the file to write.
 * @param message The message to write to the file.
 */
void writeFileFS(fs::FS &fs, const char *path, const char *message);

/**
 * @brief Get local time from NTP server.
 * 
 * @return String containing the local time.
 */
String getLocalTime();

/**
 * @brief Write data to SD card.
 * 
 * @param data The data to write to the SD card.
 */
void writeFileSD(String data);

/**
 * @brief Read temperature from sensor.
 */
void readTemp();

/**
 * @brief Get sensor data.
 * 
 * @return String containing the sensor data.
 */
String getSensorData();

/**
 * @brief Delete network settings.
 */
void deleteNetworkSettings();

/**
 * @brief Debugging function to read file from SD card.
 */
void readFileSDDEBUG();

/**
 * @brief Debugging function to reset file on SD card.
 */
void resetFileSDDEBUG();


/**
 * @brief Setup function to initialize components and start necessary services.
 * 
 * This function initializes serial communication, sensors, SPIFFS, Wi-Fi connection, and sets up routes for HTTP requests.
 */
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

    // Route for favicon
    server.on("/favicon.png", HTTP_GET, [](AsyncWebServerRequest *request)
              { request->send(SPIFFS, "/favicon.png", "image/x-icon"); });

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

/**
 * @brief Main loop function to continuously read temperature.
 * 
 * This function calls the readTemp() function repeatedly.
 */
void loop()
{
  readTemp();
}

/**
 * @brief Initialize SPIFFS (SPI Flash File System).
 * 
 * This function mounts the SPIFFS file system.
 * 
 * @return void
 */
void initSPIFFS()
{
  if (!SPIFFS.begin(true))
  {
    Serial.println("An error has occurred while mounting SPIFFS");
  }
  Serial.println("SPIFFS mounted successfully");
}

/**
 * @brief Initialize Wi-Fi connection.
 * 
 * This function initializes the Wi-Fi connection with the configured SSID and password.
 * 
 * @return bool Returns true if Wi-Fi connection is successful, false otherwise.
 */
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

/**
 * @brief Initialize SD card.
 * 
 * This function initializes the SD card and checks if the data log file exists. 
 * If not, it creates the file.
 * 
 * @return void
 */
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

/**
 * @brief Read file from SPIFFS (SPI Flash File System).
 * 
 * This function reads the content of the file located at the specified path in SPIFFS.
 * 
 * @param fs File system object (SPIFFS).
 * @param path Path of the file to be read.
 * @return String Content of the file as a string.
 */
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

/**
 * @brief Write file to SPIFFS (SPI Flash File System).
 * 
 * This function writes the specified message to the file located at the specified path in SPIFFS.
 * 
 * @param fs File system object (SPIFFS).
 * @param path Path of the file to be written.
 * @param message Message to be written to the file.
 * @return void
 */
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

/**
 * @brief Get local time.
 * 
 * This function retrieves the local time and returns it as a formatted string.
 * 
 * @return String Formatted string representing the local time.
 */
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

/**
 * @brief Write data to SD card.
 * 
 * This function appends the specified data to the data log file on the SD card.
 * 
 * @param data Data to be written to the file.
 * @return void
 */
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

/**
 * @brief Read temperature and write average to SD card.
 * 
 * This function reads the temperature from the DS18B20 sensor, calculates the average temperature, 
 * and writes it along with the current local time to the data log file on the SD card.
 * 
 * @return void
 */
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
    if (iterations == 6)
    {
      iterations = 1;
    }

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

/**
 * @brief Get sensor data from SD card and return as JSON.
 * 
 * This function retrieves sensor data from the data log file on the SD card and returns it as a JSON string.
 * 
 * @return String JSON string containing sensor data.
 */
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
    String dateStr = line.substring(commaIndex + 1, line.length());

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

/**
 * @brief Delete network settings.
 * 
 * This function deletes the network settings files stored in the SPIFFS file system.
 */
void deleteNetworkSettings()
{
  SPIFFS.remove(ssidPath);
  SPIFFS.remove(passPath);
  SPIFFS.remove(ipPath);
  SPIFFS.remove(gatewayPath);
}

/**
 * @brief Read file from SD card for debugging purposes.
 * 
 * This function reads the contents of the data log file on the SD card and prints them to the serial monitor.
 * This function is intended for debugging purposes only.
 */
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

/**
 * @brief Reset file from SD card for debugging purposes.
 * 
 * This function deletes the data log file from the SD card and creates a new empty file with the same name.
 * This function is intended for debugging purposes only.
 */
void resetFileSDDEBUG()
{
  deleteFile(SD_MMC, "/data/datalog.csv");

  writeFile(SD_MMC, "/data/datalog.csv", "");
}
