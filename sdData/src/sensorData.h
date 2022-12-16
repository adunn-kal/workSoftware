/**
 * @file sensorData.h
 * @author Alexander Dunn
 * @version 0.1
 * @date 2022-12-16
 * 
 * @copyright Copyright (c) 2022
 * 
 */

#include <Arduino.h>
#include "UnixTime.h"

class sensorData
{
    public:
        UnixTime time;
        float tempExt;
        float humExt;
        int dist;
        int fixType;

        int repr(char *buf, uint32_t n, char* unixTime); ///< A method to format and print data
};