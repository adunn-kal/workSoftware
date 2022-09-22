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
   Red - 15
   Yellow - 22
   Green - 21
   ---------------------
*/

#include <Wire.h>
#include <SD.h>
#include <DS3232RTC.h>
#include <Adafruit_SHT31.h>

// For sleep
const long CONVERSION = 1000000; // Conversion factor for seconds to microseconds
long SLEEP_TIME = 10;

// Clock variables
DS3232RTC myClock(false); //For non AVR boards (ESP32)

// Temp variables
Adafruit_SHT31 tempSensor = Adafruit_SHT31();

// For SD card
const int CS = 5; // Chip select pin

// For measurements
long myTime;
float myTemp;
float myHum;

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

  // Setup for temp sensor
  tempSensor.begin(0x44); //Hex Address for new I2C pins

  //Check for SD header file
  sdBegin();

}

void loop()
{
  // Get measurements
  getData();
  
  // Write to SD card and serial monitor
  sdWrite();

  // Go to sleep
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
  myTime = now(); // Unix time

  myTemp = tempSensor.readTemperature();
  myTemp *= 9.0;
  myTemp /= 5.0;
  myTemp += 32.0;

  myHum = tempSensor.readHumidity();
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

  //Create and open a file
  File dataFile = SD.open(fileName, FILE_WRITE);

  Serial.print("Writing ");
  Serial.print(fileName);
  Serial.print(": ");
  Serial.println(String(now()));

  //Create strings for measurement, time, and temp
  String currentTime = String(myTime);
  String currentTemp = String(myTemp);
  String currentHum = String(myHum);

  //Write time, measurement, and temp on one line in file
  dataFile.print(currentTime);
  dataFile.print(",");
  dataFile.print(currentTemp);
  dataFile.print(",");
  dataFile.println(currentHum);
  dataFile.close();

  Serial.printf("%s, %s, %s\n", currentTime, currentTemp, currentHum);
}
