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
#include "sdData.h"


File SD_Data :: createFile(char* format_buf, bool hasFix, uint32_t wakeCounter, UnixTime time, uint64_t sleep_time)
{
    // Filenames are at most 8 characters + 6("/Data/") + 4(".txt") + null terminator = 19

    // If the gps has a fix, use its timestamp
    if (hasFix || sleep_time != 0) snprintf(format_buf, 19 , "/Data/%x.txt", time);

    // If no GPS fix, use wake counter
    else snprintf(format_buf, 19, "/Data/%x_%x.txt", wakeCounter, millis());

    File file = SD.open(*format_buf, FILE_WRITE, true);
    assert(file);
    return file;
}

/**
 * @brief A method to check and write header files to the SD card
 * 
 * @param startTime The unix timestamp indicating the start time
 */
void SD_Data :: writeHeader(char* startTime)
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