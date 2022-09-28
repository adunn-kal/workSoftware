// Standalone Temperature and Humidity Sensor
// 09/21/22

/* ---------------------
   For SD card:
   MOSI (DI) - 23
   MISO (DO) - 19
   CLK - 18
   CS - 5
   ---------------------
   For RTC:
   SDA - 21
   SCL - 22
   5.01kOhm - 21 - Vcc
   5.01kOhm - 22 - Vcc
   ---------------------
   For Temp/Humidity:
   Black - GND
   Red - Vcc
   Yellow - 22
   Green - 21
   ---------------------
*/

#include <Wire.h>
#include <SD.h>
#include <DS3232RTC.h>
#include <Adafruit_SHT31.h>

// Constants
#define CONVERSION 1000000 // Conversion factor for seconds to microseconds
#define LIST_SIZE 10 // Seconds to measure for
#define SLEEP_INTERVAL 60 // Interval (in seconds) to sleep/wake

// Clock variables
DS3232RTC myClock(false); //For non AVR boards (ESP32)
int SLEEP_TIME;
const int LED = 2;

// Temp variables
Adafruit_SHT31 tempSensor = Adafruit_SHT31();

// For SD card
const int CS = 5; // Chip select pin

// For measurements
long myTime;
float myTemp;
float myHum;

long timeArray[LIST_SIZE];
float tempArray[LIST_SIZE];
float humArray[LIST_SIZE];

//-----------------------------------------------------------------------------------
void setup()
{
  Serial.begin(115200);

  //Setup for SD card
  pinMode(CS, OUTPUT);
  SD.begin(CS);

  // Setup for clock
  myClock.begin(); // Initializes I2C bus for non AVR boards
  setSyncProvider(myClock.get); // Set the external RTC as the time keeper
  pinMode(LED, OUTPUT);
  myTime = now();

  // Setup for temp sensor
  tempSensor.begin(0x44); //Hex Address for new I2C pins

  //Check for SD header file
  sdBegin();

}

void loop()
{
  if (myTime > 6000) digitalWrite(LED, HIGH);
  
  // Get measurements
  getData();
  
  // Write to SD card and serial monitor
  sdWrite();

  // Go to sleep
  digitalWrite(LED, LOW);
  updateSleep();
  Serial.printf("Sleeping for %d seconds\n\n", SLEEP_TIME);
  esp_sleep_enable_timer_wakeup(SLEEP_TIME * CONVERSION);
  Serial.flush();
  esp_deep_sleep_start();
  // Sleeps until woken, runs setup() again
}

//-----------------------------------------------------------------------------------
// Get current time, temp, and humidity
void getData()
{
  int counter = 0;
  long lastTime;

//  Serial.printf("numPoints = %d\ncounter = %d\n\n", numPoints, counter);
  
  // Loop through as many times as needed
  while (counter < LIST_SIZE)
  {    
    // One measurement per second
    if ((millis() - lastTime) > 1000)
    { 
      // Get measurements
      myTime = now(); // Unix time
    
      myTemp = tempSensor.readTemperature();
      myTemp *= 9.0;
      myTemp /= 5.0;
      myTemp += 32.0;
    
      myHum = tempSensor.readHumidity();

      // Fill arrays
      timeArray[counter] = myTime;
      tempArray[counter] = myTemp;
      humArray[counter] = myHum;

      Serial.printf("%d, %f, %f\n", myTime, myTemp, myHum);

      // Increment counter and reset timer
      counter++;
      lastTime = millis();
    }
  }
  Serial.println();
}


// Check or create header file
void sdBegin()
{
  // Check if file exists and create one if not
  if (!SD.exists("/begin.txt"))
  {
    Serial.println("Creating lead file");

    File dataFile = SD.open("/begin.txt", FILE_WRITE);

    SD.mkdir("/Data");

    // Create header with title, timestamp, and column names
    dataFile.println("Standalone Temperature and Humidity Sensor");
    dataFile.print("Starting: ");
    dataFile.println(String(now()));
    dataFile.println();
    dataFile.println("UNIX Time, Temperature (F), Humidity (%)");
    dataFile.close();
  }
}

//Write the list to the sd card
void sdWrite()
{
  //Create string for new file name
  String fileName = "/Data/";
  fileName += String(now(), HEX);
  fileName += ".txt";

  Serial.print("Writing ");
  Serial.print(fileName);
  Serial.print(": ");
  Serial.println(String(now()));

  //Create and open a file
  File dataFile = SD.open(fileName, FILE_WRITE);

  for (int i = 0; i < LIST_SIZE; i++)
  {
    //Create strings for measurement, time, and temp
    String currentTime = String(timeArray[i]);
    String currentTemp = String(tempArray[i]);
    String currentHum = String(humArray[i]);
  
    //Write time, measurement, and temp on one line in file
    dataFile.print(currentTime);
    dataFile.print(",");
    dataFile.print(currentTemp);
    dataFile.print(",");
    dataFile.println(currentHum);

    Serial.printf("%s, %s, %s\n", currentTime, currentTemp, currentHum);
  }
  dataFile.close();
}

// Update Sleep Time
void updateSleep()
{
  // Update the current minute (0-59) and convert to seconds
  int nowTime = now()%60;
  

  // Calculate sleep time based on interval
  SLEEP_TIME = SLEEP_INTERVAL - (nowTime % SLEEP_INTERVAL);

  // Offset for half the reading time
  SLEEP_TIME -= (LIST_SIZE / 2);

  // Take off one second to account for wakeup
  SLEEP_TIME -= 2;

  // If something went wrong and the sleep time is too high, sleep for the interval
  if (SLEEP_TIME > SLEEP_INTERVAL)
  {
    SLEEP_TIME = SLEEP_INTERVAL;
  }

  if (SLEEP_TIME <= 0)
  {
    // Actually only sleep for 1 second
    SLEEP_TIME = 1;
  }
}
