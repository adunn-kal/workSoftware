/**
 * @file sdData.cpp
 * @author Alexander Dunn
 * @version 0.1
 * @date 2022-12-14
 * 
 * @copyright Copyright (c) 2022
 * 
 */

#include <Arduino.h>
#include "SD.h"
#include "UnixTime.h"
#include <utility>
#include "sdData.h"


/**
 * @brief A constructor for the SD_Data class
 * 
 * @param pin The pin used for the SD chip select
 * @return SD_Data 
 */
SD_Data :: SD_Data(gpio_num_t pin)
{
    // Update CS pin
    CS = pin;

    // Start SD stuff
    pinMode(CS, OUTPUT);
    assert(SD.begin(CS));
}

/**
 * @brief A method to check and write header files to the SD card
 * 
 * @param startTime The unix timestamp indicating the start time
 */
void SD_Data :: writeHeader(char startTime)
{
    // Check if file exists and create one if not
    if (!SD.exists("/README.txt"))
    {
        File read_me = SD.open("/README.txt", FILE_WRITE);
        if(!read_me) return;

        // Create header with title, timestamp, and column names
        read_me.println("");
        read_me.printf(
            "Cal Poly Tide Sensor\n"
            "https://github.com/adunn-kal/workSoftware/tree/master/waterSense\n\n"
            "Data File format:\n"
            "Line   1: Latitude, Longitude, Altitude\n"
            "Line 'n': UNIX Time, Distance (mm), External Temp (F), Humidity (%)\n",
            startTime);
        read_me.close();

        SD.mkdir("/Data");
    }
}

/**
 * @brief Open a new file
 * 
 * @param hasFix Whether or not the GPS has a fix
 * @param wakeCounter The number of wake cycles
 * @param time The current unix timestamp
 * @param sleep_time The time in which the device must sleep
 * @return The opened file
 */
File SD_Data :: createFile(bool hasFix, uint32_t wakeCounter, UnixTime time, uint64_t sleep_time)
{
    // Filenames are at most 8 characters + 6("/Data/") + 4(".txt") + null terminator = 19
    if (hasFix || sleep_time != 0) // If the gps has a fix, use its timestamp
    {
        snprintf(format_buf, 19 , "/Data/%x.txt", time);
    }
    else // If no GPS fix, use wake counter
    {
        snprintf(format_buf, 19, "/Data/%x_%x.txt", wakeCounter, millis());
    }

    File file = SD.open(format_buf, FILE_WRITE, true);
    assert(file);
    return file;
}

/**
 * @brief A method to write a log message to the SD card
 * 
 * @param message A pointer to the message to write
 * @param month The current month (1-12)
 * @param day The day of the month (1-31)
 * @param year The last two digits of the current year
 * @param time A pointer to the formatted time of day
 * @param wakeCounter The number of times the MCU has woken from deep sleep
 * @param hasFix Whether or not the GPS has a fix
 * @param latitude The latitude as measured by the GPS
 * @param longitude The longitude as measured by the GPS
 * @param altitude The altitude as measured by the GPS
 */
void SD_Data :: writeLog(const char* message, uint8_t month, uint8_t day,
                         uint8_t year, char* time, uint32_t wakeCounter,
                         bool hasFix, float latitude, float longitude, float altitude)
{
    //Open log file and write to it
    File logFile = SD.open("/logFile.txt", FILE_WRITE);
    if(!logFile) return;
    //logFile.seek(logFile.size());
    logFile.printf("%d/%d/20%d, %s: %s\nwake count: %d\n", month, day, year, time, message, wakeCounter);
    if(hasFix) logFile.printf("%f, %f, %f\n", latitude, longitude, altitude);
    logFile.close();
}

/**
 * @brief A method to update the formatted buffer
 * 
 * @param buffer The new formatted buffer
 */
void SD_Data :: updateBuffer(const char* buffer)
{
    strcpy(format_buf, buffer);
}

/**
 * @brief A method to take a write data to the SD card
 * 
 * @param data_file A reference to the data file to be written to
 * @param distance The distance measured by the SONAR sensor
 * @param unixTime The unix timestamp for when the data was recorded
 * @param temperature The current temperature measured by the temperature and humidity sensor
 * @param humidity The current humidity measured by the temperature and humidity sensor
 * @param hasFix Whether or not the GPS has a fix
 * @return sensorData An object containing all of the data
 */
void SD_Data :: writeData(File &data_file, int32_t distance, char* unixTime, float temperature, float humidity, bool hasFix)
{
    int result = snprintf(format_buf, FORMAT_BUF_SIZE, "%s, %d, %f, %f, %d\n",
                          unixTime,
                          distance,
                          temperature,
                          humidity,
                          hasFix);

    if(result < 0) {} //Format error, do nothing
    else
    {
        // Some or all data succesfuly formatted into buffer
        data_file.write((uint8_t*)format_buf, (uint32_t)result);
    }
}