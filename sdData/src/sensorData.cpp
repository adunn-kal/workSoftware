/**
 * @file sensorData.cpp
 * @author Alexander Dunn
 * @version 0.1
 * @date 2022-12-16
 * 
 * @copyright Copyright (c) 2022
 * 
 */

#include <Arduino.h>
#include "UnixTime.h"
#include "sensorData.h"

/**
 * @brief Formats and prints sensor data
 * 
 * @param buf The buffer to format
 * @param n The buffer size
 * @param unixTime 
 * @return int The ASCII representation of the formatted buffer
 */
int sensorData :: repr(char *buf, uint32_t n, char* unixTime)
{
    return snprintf(buf, n, "%s, %d, %f, %f, %d\n",
    unixTime,
    dist,
    tempExt,
    humExt,
    fixType);
}