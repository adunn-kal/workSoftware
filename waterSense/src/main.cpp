/**
 * @file main.cpp
 * @author Alexander Dunn
 * @brief The main waterSense software file
 * @version 0.1
 * @date 2022-12-18
 * 
 * @copyright Copyright (c) 2022
 * 
 */

#include <Arduino.h>
#include <utility>
#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include <driver/timer.h>
#include <soc/rtc.h>
#include "Adafruit_GPS.h"
#include "UnixTime.h"
#include <esp_now.h>
#include <WiFi.h>

// New Libraries
#include "maxbotixSonar.h"
#include "adafruitTempHumidity.h"
#include "gpsClock.h"
#include "taskshare.h"



//-----------------------------------------------------------------------------------------------------||
//---------- Define Constants -------------------------------------------------------------------------||

#define MEASUREMENT_PERIOD 100 ///< Measurement task period in ms
#define SD_PERIOD 10 ///< SD task period in ms
#define CLOCK_PERIOD 10 ///< Clock task period in ms
#define SLEEP_PERIOD 100 ///< Sleep task period in ms

#define READ_TIME 20 ///< Length of time to measure in seconds
#define MINUTE_ALLIGN 2 ///< Allignment time in minutes

//-----------------------------------------------------------------------------------------------------||
//-----------------------------------------------------------------------------------------------------||










//-----------------------------------------------------------------------------------------------------||
//---------- Define Pins ------------------------------------------------------------------------------||

#define SD_CS GPIO_NUM_5 ///< SD card chip select pin

#define SONAR_RX GPIO_NUM_14 ///< Sonar sensor receive pin
#define SONAR_TX GPIO_NUM_32 ///< Sonar sensor transmit pin

/**
 * @brief Sonar sensor enable pin
 * @details Sonar measurements are disbale when this pin is pulled low
 * 
 */
#define SONAR_EN GPIO_NUM_33

#define GPS_RX GPIO_NUM_16 ///< GPS receive pin
#define GPS_TX GPIO_NUM_17 ///< GPS transmit pin

/**
 * @brief GPS enable pin
 * @details GPS measurements are disabled when this pin is pulled low
 * 
 */
#define GPS_EN GPIO_NUM_27

#define TEMP_SENSOR_ADDRESS 0x44 ///< Temperature and humidity sensor hex address

/**
 * @brief Temperature/humidity sensor enable pin
 * @details Temperature and humidity measurements are disabled when this pin is pulled low
 * 
 */
#define TEMP_EN GPIO_NUM_15

//-----------------------------------------------------------------------------------------------------||
//-----------------------------------------------------------------------------------------------------||










//-----------------------------------------------------------------------------------------------------||
//---------- Shares & Queues --------------------------------------------------------------------------||

// Sleep shares
Share<bool> sleepFlag("Sleep Flag"); ///< A shared variable to trigger sleep operations
Share<bool> clockSleepReady("Clock Sleep Ready"); ///< A shared variable to indicate the clock is ready to sleep
Share<bool> sonarSleepReady("Sonar Sleep Ready"); ///< A shared variable to indicate the sonar sensor is ready to sleep
Share<bool> tempSleepReady("Temp Sleep Ready"); ///< A shared variable to indicate the temp sensor is ready to sleep
Share<bool> sdSleepReady("SD Sleep Ready"); ///< A shared variable to indicate the SD card is ready to sleep

// Shares from GPS
Share<float> latitude("Latitude");
Share<float> longitude("Longitude");
Share<float> altitude("Altitude");
Share<uint8_t> fixType("Fix Type");
Share<String> unixTime("Unix Time");

//-----------------------------------------------------------------------------------------------------||
//-----------------------------------------------------------------------------------------------------||










//-----------------------------------------------------------------------------------------------------||
//---------- Class Objects ----------------------------------------------------------------------------||

MaxbotixSonar mySonar(&Serial1, SONAR_RX, SONAR_TX, SONAR_EN);
AdafruitTempHumidity myTemp(TEMP_EN, TEMP_SENSOR_ADDRESS);
GpsClock myGPS(&Serial2, GPS_RX, GPS_TX, GPS_EN);

//-----------------------------------------------------------------------------------------------------||
//-----------------------------------------------------------------------------------------------------||









//-----------------------------------------------------------------------------------------------------||
//---------- Tasks ------------------------------------------------------------------------------------||

/**
 * @brief The measurement task
 * @details Takes measurements from the sonar sensor, temperature and humidity, and GPS data
 * 
 * @param params A pointer to task parameters
 */
void taskMeasure(void* params)
{
  // Task Setup
  uint8_t state = 0;

  // Task Loop
  while (true)
  {
    // Begin
    if (state == 0)
    {
      // Setup sonar
      mySonar.begin();
      sonarSleepReady.put(false);

      // Setup temp/humidity
      myTemp.begin();
      tempSleepReady.put(false);

      state = 1;
    }

    // Check for data
    else if (state == 1)
    {
      // If data is available, go to state 2
      if (mySonar.available())
      {
        state = 2;
      }

      // If sleepFlag is tripped, go to state 3
      if (sleepFlag.get())
      {
        state = 3;
      }
    }

    // Get measurements
    else if (state == 2)
    {
      // Get sonar measurement
      Serial.print(mySonar.measure());
      Serial.print(", ");
      Serial.print(myTemp.getTemp());
      Serial.print(", ");
      Serial.println(myTemp.getHum());

      // Write all data to shares
      // Trip dataFlag

      state = 1;
    }

    // Sleep
    else if (state == 3)
    {
      // Disable sonar
      mySonar.sleep();
      sonarSleepReady.put(true);

      // Disable temp/humidity
      myTemp.sleep();
      tempSleepReady.put(true);
    }

    vTaskDelay(MEASUREMENT_PERIOD);
  }
}

/**
 * @brief The SD storage task
 * @details Creates relevant files on the SD card and stores all data
 * 
 * @param params A pointer to task parameters
 */
void taskSD(void* params)
{
  // Task Setup
  uint8_t state = 0;

  // Task Loop
  while (true)
  {
    // Begin
    if (state == 0)
    {
      // Check/create header files
      state = 1;
    }

    // Check for data
    else if (state == 1)
    {
      // If data is available, untrip dataFlag go to state 2

      // If sleepFlag is tripped, go to state 3
    }
    // Store data
    else if (state == 2)
    {
      // Get sonar data
      // Get temp data
      // Get humidity data
      // Get timestamp 

      // Write all data to SD card
      state = 1;
    }
    // Sleep
    else if (state == 3)
    {
      // Close data file
    }

    vTaskDelay(SD_PERIOD);
  }
}

/**
 * @brief The clock task
 * @details Runs the GPS clock in the background to maintain timestamps
 * 
 * @param params A pointer to task parameters
 */
void taskClock(void* params)
{
  Adafruit_GPS myClock = myGPS.begin();
  uint8_t state = 0;

  while (true)
  {
    // Begin
    if (state == 0)
    {
      // Any setup?

      state = 1;
    }

    // Read
    else if (state == 1)
    {
      // Update the GPS
      myGPS.update(myClock);

      // If new data is available, go to state 2
      if (myGPS.newData)
      {
        state = 2;
      }

      // If sleepFlag is tripped, go to state 3
      if (sleepFlag.get())
      {
        state = 3;
      }
    }

    // Update
    else if (state == 2)
    {
      myGPS.update(myClock);

      latitude.put(myGPS.latitude);
      longitude.put(myGPS.longitude);
      altitude.put(myGPS.altitude);
      fixType.put(myGPS.fixType);
      unixTime.put(myGPS.getUnix(myClock));
      
      state = 1;
    }

    // Sleep
    else if (state == 3)
    {
      // Disable sonar
      myGPS.sleep();
      clockSleepReady.put(true);
    }

    vTaskDelay(CLOCK_PERIOD);
  }
}

/**
 * @brief The sleep task
 * @details Sets the sleep time and triggers sleep
 * 
 * @param params A pointer to task parameters
 */
void taskSleep(void* params)
{
  // Task Setup
  uint8_t state = 0;
  uint64_t runTimer = millis();

  // Task Loop
  while (true)
  {
    // Begin
    if (state == 0)
    {
      // Start run timer
      runTimer = millis();

      // Make sure sleep flag is not set
      sleepFlag.put(false);

      Serial.println("Sleep state 0 -> 1");
      state = 1;
    }

    // Wait
    else if (state == 1)
    {
      // If runTimer, go to state 2
      if ((millis() - runTimer) > READ_TIME*1000)
      {
        Serial.println("Sleep state 1 -> 2");

        // Set sleep flag
        sleepFlag.put(true);
        state = 2;
      }
    }

    // Initiate Sleep
    else if (state == 2)
    {
      // If all tasks are ready to sleep, go to state 3
      if (sonarSleepReady.get() && tempSleepReady.get() && clockSleepReady.get())
      {
        Serial.println("Sleep state 2 -> 3");
        state = 3;
      }
    }

    // Sleep
    else if (state == 3)
    {
      // Go to sleep
      Serial.println("Going to sleep");
      
      uint64_t sleepTime = 10*1000000;
      gpio_deep_sleep_hold_en();
      esp_sleep_enable_timer_wakeup(sleepTime);
      esp_deep_sleep_start();

      state = 0;
    }

    vTaskDelay(SLEEP_PERIOD);
  }
}

//-----------------------------------------------------------------------------------------------------||
//-----------------------------------------------------------------------------------------------------||










//-----------------------------------------------------------------------------------------------------||
//---------- Program ----------------------------------------------------------------------------------||

void setup()
{
  // Setup
  Serial.begin(115200);
  while (!Serial) {}

  // Setup tasks
  xTaskCreate(taskMeasure, "Measurement Task", 8192, NULL, 3, NULL);
  // xTaskCreate(taskSD, "SD Task", 8192, NULL, 7, NULL);
  // xTaskCreate(taskClock, "Clock Task", 8192, NULL, 5, NULL);
  xTaskCreate(taskSleep, "Sleep Task", 8192, NULL, 1, NULL);
}

void loop()
{
  // Loop
}

//-----------------------------------------------------------------------------------------------------||
//-----------------------------------------------------------------------------------------------------||