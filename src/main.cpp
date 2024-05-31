#include <Arduino.h>
#include <Fs.h>
#include <LittleFS.h>
#include "sd_read_write.h"
#include "SD_MMC.h"

#define SD_MMC_CMD 38 // Please do not modify it.
#define SD_MMC_CLK 39 // Please do not modify it.
#define SD_MMC_D0 40  // Please do not modify it.

void setup()
{
  Serial.begin(115200);
  SD_MMC.setPins(SD_MMC_CLK, SD_MMC_CMD, SD_MMC_D0);
  if (!SD_MMC.begin("/sdcard", true, true, SDMMC_FREQ_DEFAULT, 5))
  {
    Serial.println("Card Mount Failed");
    return;
  }


  
  

}



void loop()
{
}