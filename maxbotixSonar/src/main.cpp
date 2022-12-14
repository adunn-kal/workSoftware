#include <Arduino.h>
#include "maxbotixSonar.h"

#define SONAR_RX GPIO_NUM_14 // Sonar sensor receive pin
#define SONAR_TX GPIO_NUM_32 // Sonar sensor transmit pin
#define SONAR_EN GPIO_NUM_33 //Sonar measurements are disabled when this pin is pulled low

uint8_t state = 0;
uint32_t start;

// Construct an object
MaxbotixSonar mySonar(&Serial1, SONAR_RX, SONAR_TX, SONAR_EN);

void setup()
{
  Serial.begin(115200);
  while(!Serial);
  Serial.println("Starting!");
}

void loop()
{
  if (state == 0) // Begin
  {
    Serial.println("Starting up");
    mySonar.begin();

    start = millis();
    state = 1;
  }

  else if (state == 1) // Check available
  {
    if (mySonar.available()) state = 2;
    if ((millis()-start) > 10000) state = 3;
  }

  else if (state == 2) // Measure
  {
    Serial.println(mySonar.measure());
    state = 1;
  }

  else if (state == 3) // Sleep
  {
    Serial.println("Going to sleep!");
    mySonar.sleep();
    delay(5000);
    state = 0;
  }
}