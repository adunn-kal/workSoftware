/**
 * @file sdData.h
 * @author Alexander Dunn
 * @version 0.1
 * @date 2022-12-14
 * 
 * @copyright Copyright (c) 2022
 * 
 */

#include <Arduino.h>
#include "SD.h"
#include <utility>
#include "UnixTime.h"

class SD_Data
{
    protected:
        // Protected data
        gpio_num_t CS;
        char format_buf[200];

    public:
        // Public data

        SD_Data(gpio_num_t pin); ///< A constructor for the SD_Data class

        // void writeData()

        void writeHeader(char* startTime); ///< A method to check and format header files

        /// A method to open a new file
        File createFile(bool hasFix, uint32_t wakeCounter, UnixTime time, uint64_t sleep_time);

        /// A method to write a log message
        void writeLog(const char* message, uint8_t month, uint8_t day,
                      uint8_t year, char* time, uint32_t wakeCounter,
                      bool hasFix, float latitude, float longitude, float altitude);

        void updateBuffer(const char* buffer); ///< Update the format_buf
};