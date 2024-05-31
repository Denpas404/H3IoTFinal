#include <Arduino.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <Fs.h>
#include <LittleFS.h>
#include "sd_read_write.h"
#include "SD_MMC.h"

#define SD_MMC_CMD 38 // Please do not modify it.
#define SD_MMC_CLK 39 // Please do not modify it.
#define SD_MMC_D0 40  // Please do not modify it.

// GPIO where the DS18B20 is connected to
const int oneWireBus = 4;

// Setup a oneWire instance to communicate with any OneWire devices
OneWire oneWire(oneWireBus);

// Pass our oneWire reference to Dallas Temperature sensor
DallasTemperature sensors(&oneWire);

// Define functions
void Temperatur();
void InitSDCard();

void setup()
{
  // Start the Serial Monitor
  Serial.begin(115200);

  // Start the DS18B20 sensor
  sensors.begin();

  // Initialize the SD Card
  InitSDCard();
}

void loop()
{
  Temperatur();

  vTaskDelay(5000);
}

void Temperatur()
{
  sensors.requestTemperatures();
  float temperatureC = sensors.getTempCByIndex(0);
  Serial.print(temperatureC);
  Serial.println("ºC");
}

void InitSdCard()
{
  // Start the SD Card
  SD_MMC.setPins(SD_MMC_CLK, SD_MMC_CMD, SD_MMC_D0);
  if (!SD_MMC.begin("/sdcard", true, true, SDMMC_FREQ_DEFAULT, 5))
  {
    Serial.println("Card Mount Failed");
    return;
  }
}