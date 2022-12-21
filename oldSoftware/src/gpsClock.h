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

        uint8_t freq;
        uint8_t diffGMT;

    public:
        GpsClock(HardwareSerial *ser, gpio_num_t rxPin, gpio_num_t txPin, gpio_num_t enPin, uint8_t freq); ///< Constructor

        void begin();
};