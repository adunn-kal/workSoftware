/**
 * @file gpsClock.h
 * @author Alexander Dunn
 * @version 0.1
 * @date 2022-12-21
 * 
 * @copyright Copyright (c) 2022
 * 
 */

#include <Arduino.h>
#include "Adafruit_GPS.h"
#include "UnixTime.h"

class GpsClock
{
    protected:
        uint16_t MONITOR_SPEED = 9600; ///< Serial port baud rate
        HardwareSerial* serialPort; ///< A pointer to a serial object
        gpio_num_t RX;
        gpio_num_t TX;
        gpio_num_t EN;

        uint8_t diffGMT;

    public:
        bool newData = false; ///< A flag to represent when new data has arrived
        uint32_t millisOffset = 0; ///< The millisecond offset between the the mcu and the GPS
        float latitude; ///< Signed latitude in degrees.minutes
        float longitude; ///< Signed longitude in degrees.minutes
        float altitude; ///< Altitude in meters above MSL
        
        /**
         * @brief The fix type of the reciever
         * @details 0: No fix, 1: GPS fix, 2: DGPS Fix
         * 
         */
        uint8_t fixType;

        GpsClock(HardwareSerial *ser, gpio_num_t rxPin, gpio_num_t txPin, gpio_num_t enPin); ///< Constructor

        Adafruit_GPS begin(); ///< A method to start the GPS module

        void update(Adafruit_GPS &GPS); ///< A method to run and check the GPS module for new data
        void read(Adafruit_GPS &GPS); ///< A method to read data from the GPS module
};