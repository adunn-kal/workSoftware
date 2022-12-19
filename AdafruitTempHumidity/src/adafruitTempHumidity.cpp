/**
 * @file adafruitTempHumidity.cpp
 * @author Alexander Dunn
 * @version 0.1
 * @date 2022-12-19
 * 
 * @copyright Copyright (c) 2022
 * 
 */

#include <Arduino.h>
#include "adafruitTempHumidity.h"

/**
 * @brief A constructor for the Adafruit temperature and humidity sensor class
 * 
 * @param enPin The enable pin for the sensor
 * @param hexAddress The i2c hex address of the sensor
 * @return AdafruitTempHumidity A class object
 */
AdafruitTempHumidity :: AdafruitTempHumidity(gpio_num_t enPin, uint8_t hexAddress)
{
    EN = enPin;
    ADDRESS = hexAddress;
}

/**
 * @brief An initializer for a temperature and humidity sensor object
 * 
 */
void AdafruitTempHumidity :: begin(void)
{
    gpio_hold_dis(EN);
    pinMode(EN, OUTPUT);
    digitalWrite(EN, HIGH); //Hold high to enable measuring with sonar
    
    Sensor.begin(ADDRESS); //Hex Address for new I2C pins
}

/**
 * @brief A method to put the sensor to sleep
 * 
 */
void AdafruitTempHumidity :: sleep(void)
{
    digitalWrite(EN, LOW);
    gpio_hold_en(EN);
}

/**
 * @brief A method to obtain a temperature measurement
 * 
 * @return float A temperature reading in degrees Fahrenheit
 */
float AdafruitTempHumidity :: getTemp(void)
{
    float temp = Sensor.readTemperature();
    temp *= 9.0/5.0;
    temp += 32.0;

    return temp;
}

/**
 * @brief A method to obtain a relative humidity measurement
 * 
 * @return float A relative humidity measurement in percent humidity
 */
float AdafruitTempHumidity :: getHum(void)
{
    float hum = Sensor.readHumidity();

    return hum;
}