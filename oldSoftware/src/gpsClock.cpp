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

GpsClock :: GpsClock(HardwareSerial *ser, gpio_num_t rxPin, gpio_num_t txPin, gpio_num_t enPin, uint8_t freq)
{
    serialPort = ser;
    RX = rxPin;
    TX = txPin;
    EN = enPin;
}

void GpsClock :: begin()
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
}