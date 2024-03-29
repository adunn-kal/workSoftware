// RTK Rover using NS-HP-GN5
// 09/22/22
// Data Sheet: https://navspark.mybigcommerce.com/content/AN0039.pdf 
// GPS: https://navspark.mybigcommerce.com/ns-hp-gn5-px1125r-l1-l5-rtk-breakout-board/
// LORA: https://github.com/sandeepmistry/arduino-LoRa/blob/master/API.md

  
#include <SPI.h>
#include <LoRa.h>
#include <Wire.h>
#include <SD.h>

//-------------------------------------------------------------------------
//----- Mode --------------------------------------------------------------

// Comment this line out to disable datestamps
// WARNING: Recommended if testing will last beyond midnight GMT
//#define DATE_MODE

//-------------------------------------------------------------------------










//-------------------------------------------------------------------------
//----- Declare Constants -------------------------------------------------

#define RTCM_INTERVAL 5 // ms between RTCM updates
#define NMEA_INTERVAL 2500 // ms between NMEA updates
#define USER_INTERVAL 100 // ms between user interaction
#define DATA_INTERVAL 5000 // ms between user interaction
#define ACCURACY_INTERVAL  NMEA_INTERVAL // Set accuracy interval to the same as NMEA

// Define Pins and Constants
#define LED 2
#define TRIG 13
#define ChS 15
#define NUM 60 // Standard deviation data
#define ARRAY_SIZE 700
#define FILE_SIZE 13 // # of lines per text file (one extra)
#define SPREAD_FACTOR 8 // 6-12 (default 7)
//#define WRITE_DELAY 10

// Define the pins used by the transceiver module
#define ss 5
#define rst 4
#define dio0 4

// Input RTCM from radio
#define RX1 GPIO_NUM_14 // Unused
#define TX1 GPIO_NUM_32

// Talk to GPS module, get NMEA
#define RX2 GPIO_NUM_16
#define TX2 GPIO_NUM_17

//-------------------------------------------------------------------------










//-------------------------------------------------------------------------
//----- Declare Variables -------------------------------------------------

long rtcmTimer;
long nmeaTimer;
long userTimer;
long accuracyTimer;
long dataTimer;

byte readArray[ARRAY_SIZE];
byte loraBytes[ARRAY_SIZE];
byte fullMessage[ARRAY_SIZE];
int packetNumber = 0;
int numPackets = 0;
int packetIndex = 0;
int errors = 0;

int fileCounter;
String timeArray[FILE_SIZE];
int fixArray[FILE_SIZE];
int sivArray[FILE_SIZE];
String latArray[FILE_SIZE];
String longArray[FILE_SIZE];
float altArray[FILE_SIZE];
#ifdef DATE_MODE
  String dayArray[FILE_SIZE];
  String monthArray[FILE_SIZE];
  String yearArray[FILE_SIZE];
  String dateArray[FILE_SIZE];
#endif

String GPGGA;
#ifdef DATE_MODE
  String PSTI;
#endif

float myLat;
int latMain;
int latDec;
float myLong;
int longMain;
int longDec;
float myAlt;
String myTime;
int fixType;
int numSats;
float myAcc;
#ifdef DATE_MODE
  String myDate;
  String myDay;
  String myMonth;
  String myYear;
#endif

bool nmeaStream = false;
bool rtcmStream = true;
bool loraOn = true;
bool dataStream = true;
bool timerStream = false;
bool receiving = false;

float lats[NUM];
float longs[NUM];
float alts[NUM];
int accIDX;
int arrayCounter = -1;
float meanLat;
float meanLong;
float meanAlt;
float posAcc;
float altAcc;
bool accFix = false;

//-------------------------------------------------------------------------










//-------------------------------------------------------------------------
//----- Main Program ------------------------------------------------------

void setup()
{
  // Start computer-ESP serial
  Serial.begin(115200);
  while(!Serial) {}

  pinMode(RX1, INPUT_PULLUP);
  pinMode(TX1, INPUT_PULLUP);
  pinMode(RX2, INPUT_PULLUP);
  pinMode(TX2, INPUT_PULLUP);

  // Start RTCM serial
  Serial1.begin(115200, SERIAL_8N1, RX1, TX1);
  while(!Serial1) {}

  // Start ESP-GPS serial
  Serial2.begin(115200, SERIAL_8N1, RX2, TX2);
  while(!Serial2) {}

  // Set pins
  pinMode(LED, OUTPUT);
  pinMode(TRIG, OUTPUT);
  pinMode(TRIG, LOW);
  pinMode(ChS, OUTPUT);

  SD.begin(ChS);
  sdBegin();

 // Setup LoRa transceiver module
 LoRa.setPins(ss, rst, dio0);

 //433E6 for Asia
 //866E6 for Europe
 //915E6 for North America
 while (!LoRa.begin(915E6))
 {
   Serial.println(".");
   delay(500);
 }

  LoRa.setSyncWord(0xF3);
  LoRa.setSpreadingFactor(SPREAD_FACTOR); // ranges from 6-12,default 7 (Higher is slower but better)
//  LoRa.setTxPower(20, true); // ranges from 14-20, higher takes more power but is better
  Serial.println("Starting!\n");
//  Serial1.flush();
//  Serial2.flush();
  delay(500);

  // Set LoRa module to continuous receive mode
  LoRa.receive();

  printHelp();

  packetNumber = 0;
  numPackets = 0;
  packetIndex = 0;

}

void loop()
{ 
  // Run RTCM task
  taskRTCM(); 

  // Run NMEA task
  taskNMEA();

  // Run accuracy task
  taskAccuracy();

  // Run user task
  taskUser();

  // Run data task
  taskData();
}

//-------------------------------------------------------------------------










//-------------------------------------------------------------------------
//----- Tasks -------------------------------------------------------------

void taskRTCM()
{
  if ((millis() - rtcmTimer) > RTCM_INTERVAL)
  {
    // Reset timer
    rtcmTimer = millis();
    
    // Try to parse packet
    uint8_t packetSize = LoRa.parsePacket();
    if ((packetSize>24) && loraOn)
    {
//      Serial.printf("Packet Size: %d bytes\n", packetSize);
      
      // Set recieving flag to true
      receiving = true;

      byte myPacket[packetSize];
      byte checkSum = 0;
      for (uint16_t i = 0; i < packetSize-1; i++)
      {
        byte newByte = LoRa.read();
        myPacket[i] = newByte;
        checkSum ^= newByte;
      }

      if(LoRa.read() != checkSum)
      {
        Serial.println("Message Error!");
        errors++;
        return;
      }
      
      Serial1.write(myPacket, packetSize-1);
      Serial1.flush();

      // Pause to let byte buffer write
      #ifdef WRITE_DELAY
        long start = millis();
        while ((millis() - start) < WRITE_DELAY) {}
      #endif

      if(rtcmStream)
      {
        for (uint16_t i = 0; i < packetSize-1; i++)
        {
          Serial.print(myPacket[i], HEX);
          Serial.print(" ");
        }
        Serial.println();
        Serial.printf("Message length: %d bytes\n", packetSize-1);
        Serial.println();
      }

      
      
//      // Read packet
//      String loraData;
//      while (LoRa.available())
//      {
//        loraData = LoRa.readString();
//
//        // Process the string into a byte array
//        processBytes(loraData);
//      }
    }

    if (timerStream) Serial.printf("Task RTCM: %dms\n", (millis()-rtcmTimer));
  }
}

void taskData()
{
  if ((millis() - dataTimer) > DATA_INTERVAL)
  {
    // Reset timer and increment counter
    dataTimer = millis();
    arrayCounter += 1;

    // Fill arrays
    #ifdef DATE_MODE
      dateArray[arrayCounter] = myDate;
      dayArray[arrayCounter] = myDay;
      monthArray[arrayCounter] = myMonth;
      yearArray[arrayCounter] = myYear;
    #endif
    timeArray[arrayCounter] = myTime;
    fixArray[arrayCounter] = fixType;
    sivArray[arrayCounter] = numSats;
    latArray[arrayCounter] = String(latMain) + "." + String(latDec);
    longArray[arrayCounter] = String(longMain) + "." + String(longDec);
    altArray[arrayCounter] = myAlt;

    if (arrayCounter >= FILE_SIZE-2)
    {
      arrayCounter = -1;

      // Verify that the files may be written
      sdBegin();
      sdWrite();
    }

    // Print data as it comes in if stream is on and fix is acquired
    if (dataStream and fixType)
    {
      #ifdef DATE_MODE
        Serial.printf("%s ", myDate);
      #endif
      Serial.printf("%s, %d, %d, %d.%d, %d.%d, %f\n", myTime, fixType, numSats, latMain, latDec, longMain, longDec, myAlt);
    }

    if (timerStream) Serial.printf("Task Data: %dms\n", (millis()-dataTimer));
  }
}

void taskNMEA()
{
  // Display NMEA readings at set RTCM_INTERVAL
  if ((millis() - nmeaTimer) > NMEA_INTERVAL)
  { 
    if (fixType && receiving) digitalWrite(LED, HIGH);
    
    // Reset timer
    nmeaTimer = millis();
      
    // Read all data from serial2 channel
    char nmeaBuffer[1000];
    int i = 0;
    Serial2.flush();
    while (Serial2.available())
    {
      char myChar = Serial2.read();
      nmeaBuffer[i] = myChar;
      i++;
    }
  
    String nmeaString = String(nmeaBuffer);
//    Serial.println(nmeaString);
//    Serial.println();

    GPGGA = nmeaString.substring(0, nmeaString.indexOf('\n'));

    #ifdef DATE_MODE
      PSTI = nmeaString.substring(nmeaString.indexOf("$PSTI"), nmeaString.indexOf("$PSTI")+200);
      PSTI = PSTI.substring(0, PSTI.indexOf('\n'));
    #endif
    
    if (nmeaStream)
    {
      Serial.println();
      Serial.println(nmeaString);
//      Serial.println(GPGGA);
      Serial.println();
    }
    digitalWrite(LED, LOW);

    parseGPGGA(GPGGA);
    #ifdef DATE_MODE
      parsePSTI(PSTI);
    #endif

    if (timerStream) Serial.printf("Task NMEA: %dms\n", (millis()-nmeaTimer));
  }
}

void taskUser()
{
  if ((millis() - userTimer) > USER_INTERVAL)
  {
    // Reset timer
    userTimer = millis();
    
    // Check serial port for characters
    if (Serial.available())
    {
      // Read input command
      char var = Serial.read();
      
      // Clear newline characters
      Serial.read();

      switch (var)
      {
        case 'h':
          // h: Print help message
          printHelp();
          break;

        case 'd':
          // d: Print Data
          if (dataStream)
          {
            // Turn stream off
            Serial.println("Data stream turned off");
            dataStream = false;
          }

          else
          {
            // Turn stream on
            Serial.println("Data stream turned on");
            dataStream = true;
          }
          break;

        case 'n':
          // n: Toggle NMEA data stream
          if (nmeaStream)
          {
            // Turn stream off
            Serial.println("NMEA data stream turned off");
            nmeaStream = false;
          }

          else
          {
            // Turn stream on
            Serial.println("NMEA data stream turned on");
            nmeaStream = true;
          }
          break;

        case 'r':
          // r: Toggle RTCM data stream
          if (rtcmStream)
          {
            // Turn stream off
            Serial.println("RTCM data stream turned off");
            rtcmStream = false;
          }

          else
          {
            // Turn stream on
            Serial.println("RTCM data stream turned on");
            rtcmStream = true;
          }
          break;

        case 'l':
          // l: Toggle RTCM data input over lora
          if (loraOn)
          {
            // Turn stream off
            Serial.println("RTCM correction over LoRa disabled");
            loraOn = false;
          }

          else
          {
            // Turn stream on
            Serial.println("RTCM correction over LoRa enabled");
            loraOn = true;
          }
          break;

        case 'T':
          // T: Toggle timers
          if (timerStream)
          {
            // Turn stream off
            Serial.println("Timers turned off");
            timerStream = false;
          }

          else
          {
            // Turn stream on
            Serial.println("Timers turned on");
            timerStream = true;
          }
          break;

        case 't':
          // t: Print current time
          Serial.println();
          Serial.printf("UTC Time: %s\n", myTime);
          #ifdef DATE_MODE
            Serial.printf("UTC Date: %s\n", myDate);
            Serial.printf("UTC dateTime: %s %s\n", myDate, myTime);
          #endif
          Serial.println();
          break;

        case 'p':
          // p: Print current position
          Serial.println();
          Serial.println("Latitude, Longitude, Altitude\n");
          Serial.println("Current position:");
          Serial.printf("%d.%d, %d.%d, %.2f\n", latMain, latDec, longMain, longDec, myAlt);
          Serial.println();

          if (accFix)
            {
              Serial.printf("Average position over the last %d seconds:\n", NUM);
              Serial.printf("%f, %f, %.2f\n", meanLat, meanLong, meanAlt);
              Serial.println();
            }

            else
            {
              Serial.printf("Average position data in %d seconds\n", NUM-accIDX);
              Serial.println();
            }
          break;

        case 'f':
          // f: Print fix type
          switch (fixType)
          {
            case 1:
              Serial.println();
              Serial.println("Standard GPS fix");
              Serial.println();
              break;

            case 2:
              Serial.println();
              Serial.println("Differential GPS fix");
              Serial.println();
              break;

            case 3:
              Serial.println();
              Serial.println("PPS fix");
              Serial.println();
              break;

            case 4:
              Serial.println();
              Serial.println("Full RTK fix");
              Serial.println();
              break;

            case 5:
              Serial.println();
              Serial.println("RTK float");
              Serial.println();
              break;

            default:
              Serial.println();
              Serial.println("No fix");
              Serial.println();
              break;
          }
          break;

        case 's':
          // s: Print active satellites
          Serial.println();
          Serial.printf("Active Satellites: %d\n", numSats);
          Serial.println();
          break;

        case 'a':
          // a: Print the HDOP (accuracy)
          if (myAcc < 0.001)
          {
            Serial.println();
            Serial.println("No fix");
            Serial.println();
          }

          else
          {
            if (accFix)
            {
              Serial.println();
              Serial.printf("Deviation over the last %d seconds:\n", NUM);
              Serial.printf("2D Deviation: %.2f m\n", posAcc);
              Serial.printf("Vertical Deviation: %.2f m \n", altAcc);
              Serial.println();
            }

            else
            {
              Serial.println();
              Serial.printf("Accuracy info in %d seconds\n", NUM-accIDX);
              Serial.println();
            }
            
            if (myAcc <= 1.0)
            {
              Serial.printf("HDOP: %.1f - Ideal\n", myAcc);
              Serial.println();
              break;
            }

            if (myAcc <= 2)
            {
              Serial.printf("HDOP: %.1f - Excellent\n", myAcc);
              Serial.println();
              break;
            }

            if (myAcc <= 5)
            {
              Serial.printf("HDOP: %.1f - Good\n", myAcc);
              Serial.println();
              break;
            }

            if (myAcc <= 10)
            {
              Serial.printf("HDOP: %.1f - Moderate\n", myAcc);
              Serial.println();
              break;
            }

            if (myAcc <= 20)
            {
              Serial.printf("HDOP: %.1f - Fair\n", myAcc);
              Serial.println();
              break;
            }

            if (myAcc > 20)
            {
              Serial.printf("HDOP: %.1f - Poor\n", myAcc);
              Serial.println();
              break;
            }
          }
          break;

        case 'c':
          // c: Clear all data
          Serial.println();
          Serial.printf("Clearing %d bytes of data\n", Serial2.available());
          Serial.println();
          clearPort();
          break;
          
        //----------------------------------
        
        case 'O':
          // O: Query Output
          clearPort();
          queryOutput();
          break;

        case 'F':
          // F: Query Update Frequency
          clearPort();
          queryUpdateRate();
          break;

        case 'G':
          // G: Query GPS Ephemeris
          clearPort();
          getGPS(0x01);
          break;

        case 'R':
          // R: Query RTCM output format
          clearPort();
          queryRTCM();
          break;

        case 'S':
          // S: Set RTK survey mode
          clearPort();
          setRTKSurvey(64, 4);
          break;
        
        case 'B':
          // B: Query RTK base position
          clearPort();
          queryBasePosition();
          break;

        //----------------------------------
        
        default:
          // Undefined character
          Serial.println();
          Serial.print("Undefined command: ");
          Serial.println(var);
          Serial.println();
          break;
      }
    }

    if (timerStream) Serial.printf("Task User: %dms\n", (millis()-userTimer));
  }
}

void taskAccuracy()
{
  // Only run this task if you have a fix
  if (((millis() - accuracyTimer) > ACCURACY_INTERVAL) and fixType)
  {
    // Reset timer
    accuracyTimer = millis();
    
    // Fill arrays
    lats[accIDX] = myLat;
    longs[accIDX] = myLong;
    alts[accIDX] = myAlt;

    // Increment counter
    accIDX++;
    accIDX %= NUM;
    if (accIDX == 0)
    {
      accFix = true;
    }

    // Determine means
    meanLat = 0;
    meanLong = 0;
    meanAlt = 0;
    for (int i = 0; i < NUM; i++)
    {
      meanLat += lats[i];
      meanLong += longs[i];
      meanAlt += alts[i];
    }
    meanLat /= NUM;
    meanLong /= NUM;
    meanAlt /= NUM;

    // Estimate Altitude Accuracy
    altAcc = 0;
    for (int i = 0; i < NUM; i++)
    {
      altAcc += pow((alts[i] - meanAlt), 2);
    }
    altAcc /= (NUM - 1);
    altAcc = pow(altAcc, 0.5);

    // Estimate Position Accuracy
    posAcc = 0;
    for (int i = 0; i < NUM; i++)
    {
      float dist = pow(pow((lats[i] - meanLat), 2) + pow((longs[i] - meanLong), 2), 0.5);
      posAcc += pow(dist, 2);
    }
    posAcc /= (NUM - 1);
    posAcc = pow(posAcc, 0.5);
    posAcc *= (111*1000);

    if (timerStream) Serial.printf("Task Accuracy: %dms\n", (millis()-accuracyTimer));
  }
}

//-------------------------------------------------------------------------










//-------------------------------------------------------------------------
//----- Functions ---------------------------------------------------------

void printHelp()
{
  Serial.println();
  Serial.println("||------------COMMANDS-----------||");
  Serial.println("|| h: Print this help message    ||");
  Serial.println("|| d: Toggle data stream on/off  ||");
  Serial.println("|| n: Toggle NMEA stream on/off  ||");
  Serial.println("|| r: Toggle RTCM stream on/off  ||");
  Serial.println("|| l: Enable/Disable RTCM input  ||");
  Serial.println("|| T: Enable/Disable task timers ||");
  Serial.println("|| t: Print the current time     ||");
  Serial.println("|| p: Print the current position ||");
  Serial.println("|| f: Print the current fix type ||");
  Serial.println("|| s: Print number of satellites ||");
  Serial.println("|| a: Print the HDOP (accuracy)  ||");
  Serial.println("|| c: Clear all serial data      ||");
  Serial.println("||                               ||");
  Serial.println("||----------GPS COMMANDS---------||");
  Serial.println("|| F: Query GPS update frequency ||");
  Serial.println("|| R: Query RTCM output status   ||");
  Serial.println("|| O: Query GPS output format    ||");
  Serial.println("|| G: Query GPS ephemeris data   ||");
  Serial.println("|| S: Set to RTK survey mode     ||");
  Serial.println("|| B: Query RTK base position    ||");
  Serial.println("||-------------------------------||");
  Serial.println();
}

void sdBegin()
{
  // Check if file exists and create one if not
  if (!SD.exists("/README.txt"))
  {
    Serial.println("Creating lead file");

    File dataFile = SD.open("/README.txt", FILE_WRITE);

    //Create header with title, timestamp, and column names
    dataFile.println("RTK Rover");
    dataFile.print("Spreading Factor: ");
    dataFile.println(String(SPREAD_FACTOR));
    dataFile.println("UTC Time, Fix Type, SIV, Latitude, Longitude, Altitude, Day, Month, Year, Errors");
    dataFile.close();
  }

  // Create data folder if not there
  if (!SD.exists("/Data"))
  {
    SD.mkdir("/Data");
  }
}

//Write the list to the sd card
void sdWrite()
{
  //Create string for new file name
  String fileName = "/Data/text_";
  fileName += String(fileCounter);
  fileName += ".txt";
  fileCounter++;

  //Create and open a file
  File dataFile = SD.open(fileName, FILE_WRITE);

  //Iterate over entire list
  for (int i = 0; i < FILE_SIZE-1; i++)
  {    
    //Create strings for measurement, time, and temp
    #ifdef DATE_MODE
      String currentDate = dateArray[i];
      String currentDay = dayArray[i];
      String currentMonth = monthArray[i];
      String currentYear = yearArray[i];
    #endif
    String currentTime = timeArray[i];
    String currentFix = String(fixArray[i]);
    String currentSIV = String(sivArray[i]);
    String currentLat = latArray[i];
    String currentLong = longArray[i];
    String currentAlt = String(altArray[i]);

    //Write time, measurement, and temp on one line in file
    dataFile.print(currentTime);
    dataFile.print(",");
    dataFile.print(currentFix);
    dataFile.print(",");
    dataFile.print(currentSIV);
    dataFile.print(",");
    dataFile.print(currentLat);
    dataFile.print(",");
    dataFile.print(currentLong);
    dataFile.print(",");
    dataFile.print(currentAlt);
    dataFile.print(",");
    #ifdef DATE_MODE
      dataFile.print(currentDay);
      dataFile.print(",");
      dataFile.print(currentMonth);
      dataFile.print(",");
      dataFile.print(currentYear);
      dataFile.print(",");
    #endif
    dataFile.println(String(errors));
    
  }

  //Close file
  dataFile.close();

  // Reset number of errors
  errors = 0;

  Serial.println();
  Serial.println("Writing to SD done.");
  Serial.println();
}

void clearPort()
{
  Serial2.flush();
  while (Serial2.available())
  {
    Serial2.read();
  }
}

void parseGPGGA(String sentence)
{
  // If it's a valid sentence
  if ((sentence.length() > 80) and (sentence.substring(0, 6) == "$GPGGA"))
  {
//    Serial.println();
//    Serial.println(sentence);

    // Parse time
    String timeSentence = sentence.substring(7, 17);
    myTime = timeSentence.substring(0,2) + ":" + timeSentence.substring(2,4);
    myTime += ":" + timeSentence.substring(4,-1);
//    Serial.printf("Time: %s\n", myTime);

    // Parse latitude
    latMain = sentence.substring(18,20).toInt();
    latDec = (sentence.substring(20,22).toInt() * 10000000) / 60;
    latDec += sentence.substring(23,30).toInt() / 60;
    if (sentence.substring(31,32) == "S")
    {
      latMain *= -1;
    }
    
    myLat = sentence.substring(18,20).toFloat();
    myLat += sentence.substring(20,22).toFloat()/60.0;
    myLat += sentence.substring(22,30).toFloat()/60.0;
    if (sentence.substring(31,32) == "S")
    {
      myLat *= -1.0;
    }
//    Serial.printf("Latitude: %d.%d\n", latMain, latDec);

    // Parse longitude
    // 12039.9038019,W
    longMain = sentence.substring(33, 36).toInt();
    longDec = (sentence.substring(36, 38).toInt() * 10000000) / 60;
    longDec += sentence.substring(39, 46).toInt() / 60;
    if (sentence.substring(47,48) == "W") // W
    {
      longMain *= -1;
    }

    myLong = sentence.substring(33, 36).toFloat();
    myLong += sentence.substring(36, 38).toFloat()/60.0;
    myLong += sentence.substring(38, 46).toFloat()/60.0;
    if (sentence.substring(47,48) == "W")
    {
      myLong *= -1.0;
    }
//    Serial.printf("Longitude: %d.%d\n", longMain, longDec);

    // Parse altitude
    myAlt = sentence.substring(58, 64).toFloat();
//    Serial.printf("Altitude: %f\n", myAlt);

    // Parse fix type
    fixType = sentence.substring(49, 50).toInt();
//    Serial.printf("Fix Type: %d\n", fixType);

    // Parse number of satellites
    numSats = sentence.substring(51, 53).toInt();
//    Serial.printf("Active Satellites: %d\n", numSats);

    // Parse Horizontal Dilution of Precision "HDOP"
    myAcc = sentence.substring(54, 57).toFloat();
  }
}

#ifdef DATE_MODE
void parsePSTI(String sentence)
{  
  // https://navspark.mybigcommerce.com/content/PX1122R_DS.pdf
  // If it's a valid sentence (we want 032)
  if ((sentence.substring(6, 9) == "032") and (sentence.substring(0, 5) == "$PSTI"))
  {
//    Serial.println();
//    Serial.println(sentence);
//    Serial.printf("Length = %d\n", sentence.length());

    // Parse time
    String dateSentence = sentence.substring(sentence.indexOf(",", 10)+1, sentence.length());
    dateSentence = dateSentence.substring(0, dateSentence.indexOf(","));
//    Serial.println(dateSentence);

    myDate = dateSentence.substring(0,2) + "/" + dateSentence.substring(2,4) + "/20" + dateSentence.substring(4,6); // dd/mm/yyyy

    myDay = dateSentence.substring(0,2);
    myMonth = dateSentence.substring(2,4);
    myYear = "20" + dateSentence.substring(4,6);
    
//    Serial.printf("Date: %s\n", myDate);
  }
}
#endif

void getTime()
{
  digitalWrite(TRIG, LOW);
  delayMicroseconds(800);
  digitalWrite(TRIG, HIGH);
  delayMicroseconds(800);
  digitalWrite(TRIG, LOW);
  Serial2.flush();
  
  while(!Serial2.available());
  
  // Read all data from serial2 channel
  Serial2.flush();
  Serial.println();
  Serial.println("Time:");
  byte byteBuffer[Serial2.available()];
  char charBuffer[Serial2.available()];
  int i = 0;
  while (Serial2.available())
  {
    byte myChar = Serial2.read();
    byteBuffer[i] = myChar;
    charBuffer[i] = char(myChar);
    i++;
  }

  for (int idx = 0; idx < sizeof(byteBuffer); idx++)
  {
    Serial.print(byteBuffer[idx], HEX);
    Serial.print(" ");
  }
  Serial.println(String(charBuffer));
  Serial.println("\n");
}

void processBytes(String myString)
{
  // Clear loraBytes
  for (int idx = 0; idx < ARRAY_SIZE; idx++)
  {
    loraBytes[idx] = 0x00;
  }
  
  // While there are still spaces
  int i = 0;
  while (myString.indexOf(" ") > 0)
  {
    // Get the next byte in the string and add it to the byte array
//    byte myByte = myString.substring(0, myString.indexOf(" "));
    String mySect =  myString.substring(0, myString.indexOf(" "));
//    Serial.printf("mySect: %s\n", mySect);

    // Convert Hex String to a byte
    byte myByte = 0;
    int myLength = mySect.length();
    for (int idx = 0; idx < myLength; idx++)
    {
      String myChar = mySect.substring(idx, idx+1);
//      Serial.printf("Index: %d\n", idx);
//      Serial.printf("Power: %d\n", myLength-idx-1);
//      Serial.printf("myChar: %s\n", myChar);
      if (myChar == "a")
      {
        myByte += 10*pow(16, myLength-idx-1);
      }

      else
      {
        if (myChar == "b")
        {
          myByte += 11*pow(16, myLength-idx-1);
        }

        else
        {
          if (myChar == "c")
          {
            myByte += 12*pow(16, myLength-idx-1);
          }

          else
          {
            if (myChar == "d")
            {
              myByte += 13*pow(16, myLength-idx-1);
            }

            else
            {
              if (myChar == "e")
              {
                myByte += 14*pow(16, myLength-idx-1);
              }

              else
              {
                if (myChar == "f")
                {
                  myByte += 15*pow(16, myLength-idx-1);
                }

                else
                {
                  myByte += myChar.toInt()*pow(16, myLength-idx-1);
                }
              }
            }
          }
        }
      }

//      Serial.print("myByte so far: ");
//      Serial.println(myByte, HEX);
//      Serial.println();
    }
//    Serial.print("myByte: ");
//    Serial.println(myByte, HEX);
    loraBytes[i] = myByte;
    i++;

    // Remove that byte from string
    myString = myString.substring(myString.indexOf(" ")+1);
  }

  byte myBytes[i];
  for (int j = 0; j < i; j++)
  {
    myBytes[j] = loraBytes[j];
  }

  // Append messages to fullMessage
  //--------------------------------------------------
  // If the message is part of a larger message
  if ((myBytes[0] == 0xa0) and (sizeof(myBytes)>3))
  {
    bool resetFlag = false;
  
    // If we got the next packet correctly, update packet information
    if (myBytes[1] == packetNumber+1)
    {
      packetNumber = myBytes[1];
      numPackets = myBytes[2];

//      Serial.printf("Processing message %d of %d\n", packetNumber, numPackets);
    }

    // Otherwise, we did not get the packet we expected
    else
    {
      // If it's the same message as before, just ignore it
      if (myBytes[1] == packetNumber)
      {
        // Do nothing
      }

      // Otherwise, reset everything
      else
      {
        Serial.println("Got screwed up in processBytes()");
        Serial.printf("Expected message #%d\n", packetNumber+1);
        Serial.printf("Got #%d\n\n", myBytes[1]);
  
        // Reset the packet info and index
        packetNumber = 0;
        numPackets = 0;
        packetIndex = 0;

        resetFlag = true;
      }
    }

    // If we have a valid partial message
    if (!resetFlag)
    {
      // Input packet data into fullMessage
      for (int j = 3; j < sizeof(myBytes); j++)
      {
        fullMessage[packetIndex] = myBytes[j];
        packetIndex++;
      }
    }

    // If we screwed up the message somehow
    else
    {
      Serial.println("Resetting full message\n");
      
      for (int i = 0; i < ARRAY_SIZE; i++)
      {
        fullMessage[i] = 0;
      }
    }

    // Only write data when the entire message has been created
    if (packetNumber == numPackets)
    {
//      Serial.println("Writing full message!\n");
      
      if (rtcmStream)
      {
        for (int j = 0; j < packetIndex; j++)
        {
          Serial.print(fullMessage[j], HEX);
          Serial.print(" ");
        }
        Serial.println();
        Serial.printf("Message length: %d bytes\n", packetIndex);
        Serial.println();
      }
      
      if (packetIndex > 23) Serial1.write(fullMessage, packetIndex);

      // Reset the packet info and index
      packetNumber = 0;
      numPackets = 0;
      packetIndex = 0;
    }
  }
  //--------------------------------------------------

  // It's a full normal message
  else
  {
    if (rtcmStream)
    {
      Serial.println();
      for (int j = 0; j < sizeof(myBytes); j++)
      {
        Serial.print(myBytes[j], HEX);
        Serial.print(" ");
      }
      Serial.println();
      Serial.printf("Message length: %d bytes\n", sizeof(myBytes));
      Serial.println();
    }
    
    Serial1.write(myBytes, sizeof(myBytes));
  }
  
}

//-------------------------------------------------------------------------










//-------------------------------------------------------------------------
//----- GPS Commands ------------------------------------------------------

bool printBuffer(byte byteBuffer[], int buffSize)
{
  if (byteBuffer[4] == 0x83)
  {
    Serial.println();
    Serial.println("ACK message received:");
    for (int idx = 0; idx < 9; idx++)
    {
      Serial.print(byteBuffer[idx], HEX);
      Serial.print(" ");
    }
    Serial.println("\n");
      
    for (int idx = 9; idx < buffSize; idx++)
    {
      Serial.print(byteBuffer[idx], HEX);
      Serial.print(" ");
    }
    Serial.println("\n");
    
    return true;
  }
  
  else
  {
    if (byteBuffer[4] == 0x84)
    {
      Serial.println();
      Serial.println("NACK message received:");
      for (int idx = 0; idx < buffSize; idx++)
      {
        Serial.print(byteBuffer[idx], HEX);
        Serial.print(" ");
      }
      Serial.println("\n");
    }

    else
    {
      Serial.println();
      Serial.println("Unable to interpret response:");
      Serial.println();
    }

    return false;
  }
}

void dataFormat(byte format)
{
  // byte CS = 0x09 + format + 0x00;
  byte CS = 0x09 ^ format ^ 0x00;
  
  // NMEA:                                         0x01
  // Binary:                                       0x02                          CS
  byte message[] = {0xA0, 0xA1, 0x00, 0x03, 0x09, format, 0x00, CS, 0x0D, 0x0A};
  Serial2.write(message, sizeof(message));
  Serial2.flush();
  
  delay(50);
  
  // Read all data from serial2 channel
  Serial2.flush();
  Serial.println();
  Serial.println("Data Format:");
  byte byteBuffer[Serial2.available()];
  int i = 0;
  while (Serial2.available())
  {
    byte myChar = Serial2.read();
    byteBuffer[i] = myChar;
    i++;
  }

  for (int idx = 0; idx < sizeof(byteBuffer); idx++)
  {
    Serial.print(byteBuffer[idx], HEX);
    Serial.print(" ");
  }
  Serial.println("\n");
  
}

void updateRate(byte rate)
{
  // byte CS = 0x0E + rate;
  byte CS = 0x0E ^ rate;
  
  // 1Hz: 0x01
  // 50Hz: 0x50
  byte message[] = {0xA0, 0xA1, 0x00, 0x03, 0x0E, rate, 0x00, CS, 0x0D, 0x0A};
  Serial2.write(message, sizeof(message));
  Serial2.flush();

  delay(50);

  // Read all data from serial2 channel
  Serial2.flush();
  Serial.println();
  Serial.println("Update Rate:");
  byte byteBuffer[Serial2.available()];
  int i = 0;
  while (Serial2.available())
  {
    byte myChar = Serial2.read();
    byteBuffer[i] = myChar;
    i++;
  }

  for (int idx = 0; idx < sizeof(byteBuffer); idx++)
  {
    Serial.print(byteBuffer[idx], HEX);
    Serial.print(" ");
  }
  Serial.println("\n");
}

void getGPS(byte SV)
{
  byte CS = 0x30 + SV;
  
  // All SV's: 0x00
  // Particular SV: 0x(01 - 32)
  byte message[] = {0xA0, 0xA1, 0x00, 0x02, 0x30, SV, CS, 0x0D, 0x0A};
  Serial2.write(message, sizeof(message));
  Serial2.flush();
  
  delay(50);

  // Read all data from serial2 channel
  Serial2.flush();
  Serial.println();
  Serial.print("GPS Ephemeris ");
  Serial.print(SV);
  Serial.println(":");
  byte byteBuffer[Serial2.available()];
  int i = 0;
  while (Serial2.available())
  {
    byte myChar = Serial2.read();
    byteBuffer[i] = myChar;
    i++;
  }

  for (int idx = 0; idx < sizeof(byteBuffer); idx++)
  {
    Serial.print(byteBuffer[idx], HEX);
    Serial.print(" ");
  }
  Serial.println("\n");
}

void getGLONASS(byte SV)
{
  byte CS = 0x5B + SV;
  
  // All SV's: 0x00
  // Particular SV: 0x(01 - 32)
  byte message[] = {0xA0, 0xA1, 0x00, 0x02, 0x5B, SV, CS, 0x0D, 0x0A};
  Serial2.write(message, sizeof(message));
  Serial2.flush();
  
  delay(50);

  // Read all data from serial2 channel
  Serial2.flush();
  Serial.println();
  Serial.println("GLONASS Ephemeris:");
  byte byteBuffer[Serial2.available()];
  int i = 0;
  while (Serial2.available())
  {
    byte myChar = Serial2.read();
    byteBuffer[i] = myChar;
    i++;
  }

  for (int idx = 0; idx < sizeof(byteBuffer); idx++)
  {
    Serial.print(byteBuffer[idx], HEX);
    Serial.print(" ");
  }
  Serial.println("\n");
}

void queryUpdateRate()
{
  byte message[] = {0xA0, 0xA1, 0x00, 0x01, 0x10, 0x10, 0x0D, 0x0A};
  Serial2.write(message, sizeof(message));
  Serial2.flush();

  
  delay(50);

  // Read all data from serial2 channel
  Serial2.flush();
  Serial.println();
  Serial.println("Queried Update Rate:");
  byte byteBuffer[Serial2.available()];
  int i = 0;
  while (Serial2.available())
  {
    byte myChar = Serial2.read();
    byteBuffer[i] = myChar;
    i++;
  }

  printBuffer(byteBuffer, sizeof(byteBuffer));
}

void queryRTCM()
{
  byte message[] = {0xA0, 0xA1, 0x00, 0x01, 0x21, 0x21, 0x0D, 0x0A};
  Serial2.write(message, sizeof(message));
  Serial2.flush();

  
  delay(50);

  // Read all data from serial2 channel
  Serial2.flush();
  Serial.println();
  Serial.println("Queried RTCM Output Format:");
  byte byteBuffer[Serial2.available()];
  int i = 0;
  while (Serial2.available())
  {
    byte myChar = Serial2.read();
    byteBuffer[i] = myChar;
    i++;
  }

  printBuffer(byteBuffer, sizeof(byteBuffer));
}

void queryBasePosition()
{
  byte message[] = {0xA0, 0xA1, 0x00, 0x01, 0x23, 0x23, 0x0D, 0x0A};
  Serial2.write(message, sizeof(message));
  Serial2.flush();

  
  delay(50);

  // Read all data from serial2 channel
  Serial2.flush();
  Serial.println();
  Serial.println("Queried Base Position:");
  byte byteBuffer[Serial2.available()];
  int i = 0;
  while (Serial2.available())
  {
    byte myChar = Serial2.read();
    byteBuffer[i] = myChar;
    i++;
  }

  printBuffer(byteBuffer, sizeof(byteBuffer));
}

void queryOutput()
{
  byte message[] = {0xA0, 0xA1, 0x00, 0x01, 0x1F, 0x1F, 0x0D, 0x0A};
  Serial2.write(message, sizeof(message));
  Serial2.flush();

  
  delay(50);

  // Read all data from serial2 channel
  Serial2.flush();
  Serial.println();
  Serial.println("Queried Output:");
  byte byteBuffer[Serial2.available()];
  int i = 0;
  while (Serial2.available())
  {
    byte myChar = Serial2.read();
    byteBuffer[i] = myChar;
    i++;
  }

  printBuffer(byteBuffer, sizeof(byteBuffer));
}

void setRTKSurvey(long surveyLength, int surveyDeviation)
{
  byte d0,d1,d2,d3,l0,l1,l2,l3;
  
  d0 = 0x04; // 4 meters
  l0 = 0x40; // 64 seconds

  byte CS = 0x22+0x01 + l0+l1+l2+l3 +d0+d1+d2+d3;
  
  //                                             survey
  byte message[] = {0xA0, 0xA1, 0x00, 0x1F, 0x22, 0x01,
                    l3, l2, l1, l0, // Survey Length
                    d3, d2, d1, d0, // Survey Deviation
                    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // Latitude
                    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // Longitude
                    0x00, 0x00, 0x00, 0x00, 0x00, CS, 0x0D, 0x0A};

  Serial2.write(message, sizeof(message));
  Serial2.flush();

  delay(50);

  // Read all data from serial2 channel
  Serial2.flush();
  Serial.println();
  Serial.println("Changing to RTK survey mode:");
  byte byteBuffer[Serial2.available()];
  int i = 0;
  while (Serial2.available())
  {
    byte myChar = Serial2.read();
    byteBuffer[i] = myChar;
    i++;
  }

  bool received = printBuffer(byteBuffer, sizeof(byteBuffer));

  if (received)
  {
    Serial.println();
    Serial.printf("Self surveying for %d seconds\n", surveyLength);
    Serial.println();
  }
}
