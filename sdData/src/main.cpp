#include <Arduino.h>
#include "sdData.h"
#include "UnixTime.h"

#define FORMAT_BUF_SIZE 200

SD_Data myCard(GPIO_NUM_5);
File myFile;

char format_buf[FORMAT_BUF_SIZE];
int startTime;

int32_t distance = 500;
float temp = 32.2;
float hum = 45.5;

UnixTime uTime = 1671221899;




// Create a unix timestamp of the current time including milliseconds
char* unixTime(UnixTime now)
{
    snprintf(format_buf, FORMAT_BUF_SIZE, "%d.%03d", now.getUnix(), (millis() - 500)%1000);
    return format_buf;
}

// Return a current unix timestamp
UnixTime getTime(void)
{
    UnixTime stamp(500);

    // If the GPS has a fix, use it to create a timestamp
    stamp.setDateTime(2000 + 22, 12, 16, 13, 35, 30);

    return stamp;
}



void setup()
{
  Serial.begin(115200);
  while(!Serial) {}
  delay(2000);
  startTime = snprintf(format_buf, FORMAT_BUF_SIZE, "123.45");

  myCard.writeHeader(startTime);
  Serial.println("Done writing header file");
  delay(2000);

  myFile = myCard.createFile(1, 30, uTime, 60*1000);
  Serial.println("Done creating file");

}

void loop()
{
  
  for (int i = 0; i < 10; i++)
  {
    delay(1000);
    myCard.writeData(myFile, distance, unixTime(getTime()), temp, hum, 1);
    Serial.printf("Done writing file %d\n", i+1);
  }
  myFile.close();
  Serial.println("Done writing");
  delay(10000);

}