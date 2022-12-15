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

class SD_Data
{
    protected:
        // Protected data
        gpio_num_t CS;


    public:
        // Public data

        // void writeData()

        /// A method to open a new file
        File createFile(char* format_buf, bool hasFix, uint32_t wakeCounter, UnixTime time, uint64_t sleep_time);

        void writeHeader(char* startTime); ///< A method to check and format header files

        /// A method to write a log message
        void writeLog(const char* message, uint8_t month, uint8_t day,
                      uint8_t year, char* time, uint32_t wakeCounter,
                      bool hasFix, float latitude, float longitude, float altitude);
};