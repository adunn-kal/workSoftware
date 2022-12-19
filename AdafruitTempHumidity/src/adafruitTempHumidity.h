/**
 * @file adafruitTempHumidity.h
 * @author Alexander Dunn
 * @version 0.1
 * @date 2022-12-19
 * 
 * @copyright Copyright (c) 2022
 * 
 */

#include <Arduino.h>
#include "Adafruit_SHT31.h"

class AdafruitTempHumidity
{
    protected:
        // Any protected class variables go here
        gpio_num_t EN;
        uint8_t ADDRESS;

        Adafruit_SHT31 Sensor = Adafruit_SHT31();

    public:
        // Public class attributes and methods

        AdafruitTempHumidity(gpio_num_t enPin, uint8_t hexAddress); ///< Constructor

        void begin(void); ///< A method to initialize the class objects
        void sleep(void); ///< A method to put the sensor to sleep
        float getTemp(void); ///< A method for taking temperature measurements
        float getHum(void); ///< A method for taking humidity measurements
};