/**
 * @file maxbotixSonar.h
 * @author Alexander Dunn
 * @version 0.1
 * @date 2022-12-14
 * 
 * @copyright Copyright (c) 2022
 * 
 */

#include <Arduino.h>

class MaxbotixSonar
{
    protected:
        // Any protected class variables go here
        uint8_t LED_BUILTIN = GPIO_NUM_2; ///< Builtin LED pin for MCU
        uint16_t MONITOR_SPEED = 9600; ///< Serial port baud rate
        HardwareSerial* serialPort; ///< A pointer to a serial object

        gpio_num_t RX;
        gpio_num_t TX;
        gpio_num_t EN;

    public:
        // Public class attributes and methods

        MaxbotixSonar(HardwareSerial *ser, gpio_num_t rxPin, gpio_num_t txPin, gpio_num_t enPin); ///< Constructor

        void begin(void); ///< A method to initialize the class objects
        void sleep(void); ///< A method to put the sensor to sleep
        bool available(void); ///< A method to check for measurement availabilty
        int32_t measure(void); ///< A method for taking sonar measurements
};