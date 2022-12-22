/**
 * @file gpsClock.cpp
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
#include "gpsClock.h"

/**
 * @brief A constructor for an object of the gpsClock class
 * 
 * @param ser A pointer to the serial port to be used by the GPS module
 * @param rxPin The receive pin of the GPS module
 * @param txPin The transmit pin of the GPS module
 * @param enPin The enable pin of the GPS module
 * @return GpsClock A gpsClock object
 */
GpsClock :: GpsClock(HardwareSerial *ser, gpio_num_t rxPin, gpio_num_t txPin, gpio_num_t enPin)
{
    serialPort = ser;
    RX = rxPin;
    TX = txPin;
    EN = enPin;
}

/**
 * @brief A method to start the GPS module
 * @details Enables only the RMC NMEA data for speed purposes
 * 
 * @return Adafruit_GPS A GPS object to be used for data collection
 */
Adafruit_GPS GpsClock :: begin()
{
    // Start serial port
    serialPort->begin(MONITOR_SPEED, SERIAL_8N1, RX, TX);
    Adafruit_GPS GPS(serialPort);

    gpio_hold_dis(EN);
    pinMode(EN, OUTPUT);
    digitalWrite(EN, HIGH);

    // 9600 NMEA is the default baud rate for Adafruit MTK GPS's- some use 4800
    GPS.begin(9600);

    // Uncomment this line to turn on RMC (recommended minimum) and GGA (fix data) including altitude
    GPS.sendCommand(PMTK_SET_NMEA_OUTPUT_RMCGGA);
    //gps.sendCommand(PMTK_SET_NMEA_OUTPUT_RMCONLY);

    // Set the update rate
    GPS.sendCommand(PMTK_SET_NMEA_UPDATE_10HZ);

    return GPS;
}

/**
 * @brief A method to run and check the GPS module for new data
 * 
 * @param GPS A reference to a GPS object to update
 */
void GpsClock :: update(Adafruit_GPS &GPS)
{
    // Check the GPS for new messages
    GPS.read();

    // If a new message has arrived
    if(GPS.newNMEAreceived())
    {
        // Parse the message
        GPS.parse(GPS.lastNMEA());
        newData = true;

        millisOffset = (millis() - GPS.milliseconds) % 1000; // Update the millisecond offset

        fixType = GPS.fixquality; // Update the fix type

        if (GPS.lat == 'N') latitude = GPS.latitude; // Update the latitude
        else latitude = -1 * GPS.latitude;

        if (GPS.lon == 'E') longitude = GPS.longitude; // Update the longitude
        else longitude = -1 * GPS.longitude;

        altitude = GPS.altitude; // Update the latitude
    }
}

/**
 * @brief A method to read data from the GPS
 * @details only runs if new data is available
 * 
 * @param GPS A reference to a GPS object to update
 */
void GpsClock :: read(Adafruit_GPS &GPS)
{
    if (newData)
    {
        newData = false;
        
        millisOffset = (millis() - GPS.milliseconds) % 1000; // Update the millisecond offset

        fixType = GPS.fixquality; // Update the fix type

        if (GPS.lat == 'N') latitude = GPS.latitude; // Update the latitude
        else latitude = -1 * GPS.latitude;

        if (GPS.lon == 'E') longitude = GPS.longitude; // Update the longitude
        else longitude = -1 * GPS.longitude;

        altitude = GPS.altitude; // Update the latitude
    }
}