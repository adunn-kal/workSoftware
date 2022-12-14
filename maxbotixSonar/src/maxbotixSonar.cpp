/**
 * @file maxbotixSonar.cpp
 * @author Alexander Dunn
 * @version 0.1
 * @date 2022-12-14
 * 
 * @copyright Copyright (c) 2022
 * 
 */

#include <Arduino.h>
#include "maxbotixSonar.h"

/**
 * @brief A constructor for the Maxbotix Sonar class
 * 
 * @param ser A reference to a serial object
 * @param rxPin The RX pin for the serial port
 * @param txPin The TX pin for the serial port
 * @param enPin The enable pin for the sensor
 * @return MaxbotixSonar A class object
 */
MaxbotixSonar :: MaxbotixSonar(HardwareSerial *ser, gpio_num_t rxPin, gpio_num_t txPin, gpio_num_t enPin)
{
    serialPort = ser;
    RX = rxPin;
    TX = txPin;
    EN = enPin;
}

/**
 * @brief An initializer for a sonar object
 * 
 */
void MaxbotixSonar :: begin(void)
{
    gpio_hold_dis(EN);
    pinMode(EN, OUTPUT);
    digitalWrite(EN, HIGH); //Hold high to enable measuring with sonar
    serialPort->begin(MONITOR_SPEED, SERIAL_8N1, RX, TX);
}

/**
 * @brief A method to put the sensor to sleep
 * 
 */
void MaxbotixSonar :: sleep(void)
{
    digitalWrite(EN, LOW);
}

/**
 * @brief A method to check for new available data
 * 
 * @return true Indicates new data is available
 * @return false Indicates no new data is available
 */
bool MaxbotixSonar :: available(void)
{
    serialPort->flush();
    return serialPort->available();
}

/**
 * @brief A method to obtain a distance measurement from the sensor
 * 
 * @return int32_t distance in millimeters
 */
int32_t MaxbotixSonar :: measure(void)
{
    char inData[5] = {0}; //char array to read data into

    // Maxbotix reports "Rxxxx\L", where xxxx is a 4 digit mm distance, '\L' is carriage return
    // a measurement is 6 bytes
    if(serialPort->available() <= 5) return -1;
    //Measurements begin with 'R'
    while(serialPort->read() != 'R')
    {
        if(serialPort->available() <= 5) return -2;
    }

    serialPort->readBytes(inData, 4);
    if(serialPort->read() != 13) return -3; //check and discard carriage return

    // Convert character array to an integer number
    uint32_t result = atoi(inData);

    // Turn on LED if measuring properly
    // Maxbotix reports 500 if object is too close and 9999 is too far
    // Or you'll get 0 if wired improperly
    if ((result > 500) and (result < 9999))  digitalWrite(LED_BUILTIN, HIGH);
    else digitalWrite(LED_BUILTIN, LOW);

    return result;
}
