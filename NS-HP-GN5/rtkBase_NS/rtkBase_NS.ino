// RTK Base using NS-HP-GN5
// 09/16/22
// GPS: https://navspark.mybigcommerce.com/ns-hp-gn5-px1125r-l1-l5-rtk-breakout-board/
// LORA: https://github.com/sandeepmistry/arduino-LoRa/blob/master/API.md

#include <SPI.h>
#include <LoRa.h>
#include <Wire.h>
#include <SD.h>

//-------------------------------------------------------------------------
//----- Configuration -----------------------------------------------------

// Comment out this line to deactivate SD card storage
//#define SD_ACTIVE

//-------------------------------------------------------------------------










//-------------------------------------------------------------------------
//----- Declare Constants -------------------------------------------------

#define RTCM_INTERVAL 1000 // ms between RTCM updates
#define LORA_INTERVAL 10 // ms between NMEA updates
#define USER_INTERVAL 250 // ms between user interaction
#define SD_INTERVAL 60*1000 // ms between SD card writing

#define MAX_RTCM 255
#define ARRAY_SIZE 1000
#define TRY_SEND 2 // How many times to send partial rtcm messages
#define SPREAD_FACTOR 8 // 6-12 (default 7)
#define LORA_DELAY 50 // ms to delay between lora messages

// Define Pins and Constants
#define LED 2
#define TRIG 13
#define ChS 15

// Define the pins used by the transceiver module
#define ss 5
#define rst 4
#define dio0 4

// Talk to GPS module, get RTCM
#define RX2 16
#define TX2 17

//-------------------------------------------------------------------------










//-------------------------------------------------------------------------
//----- Declare Variables -------------------------------------------------

uint16_t rtcmTimer;
uint16_t loraTimer;
uint16_t userTimer;
uint16_t sdTimer;

uint16_t timer = 0;

uint32_t fileCounter = 0;
uint8_t savedBaseMode = 0;
uint32_t savedSurveyLength = 0;
uint32_t standardDeviation = 0;
double savedLat = 0.0;
double savedLong = 0.0;
float savedAlt = 0.0;
uint8_t baseMode = 0;
uint32_t surveyLength = 0;



byte rtcmBytes[ARRAY_SIZE];
uint16_t rtcmLength;

uint8_t numRTCM;
uint8_t rtcmIDXs[ARRAY_SIZE];

bool rtcmStream = false;
bool loraStream = true;
bool LoraBytes = true;
bool queryBytes = false;
bool timerStream = false;

//-------------------------------------------------------------------------










//-------------------------------------------------------------------------
//----- Main Program ------------------------------------------------------

void setup()
{
  // Start computer-ESP serial
  Serial.begin(115200);
  while(!Serial) {};

  pinMode(RX2, INPUT_PULLUP);
  pinMode(TX2, INPUT_PULLUP);

  // Start ESP-GPS serial
  Serial2.begin(115200, SERIAL_8N1, RX2, TX2);

  // Set pins
  pinMode(LED, OUTPUT);
  pinMode(TRIG, OUTPUT);
  pinMode(TRIG, LOW);

  #ifdef SD_ACTIVE
    pinMode(ChS, OUTPUT);
    SD.begin(ChS);
  #endif

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
  Serial2.flush();
  delay(500);

  // Data Sheet:
  // https://navspark.mybigcommerce.com/content/AN0039.pdf

  printHelp();

}

void loop()
{
  // Run RTCM task
  taskRTCM();

  // Run LORA task
  taskLORA();

  // Run user task
  taskUser();

  // Run SD task
  #ifdef SD_ACTIVE
    taskSD();
  #endif
}

//-------------------------------------------------------------------------










//-------------------------------------------------------------------------
//----- Tasks -------------------------------------------------------------

// Get RTCM readings at set RTCM_INTERVAL
void taskRTCM()
{
  if ((millis() - rtcmTimer) > RTCM_INTERVAL)
  {
    float period = (millis() - timer)/1000.0;
    Serial.printf("%0.3f seconds since last run\n", period);
    // Reset timer
    rtcmTimer = millis();

    // Read all data from serial2 channel
    char rtcmBuffer[1000];
    uint16_t i = 0;
    Serial2.flush();
    while (Serial2.available())
    {
      byte myByte = Serial2.read();
      rtcmBuffer[i] = char(myByte);
      rtcmBytes[i] = myByte;
      i++;
      Serial2.flush();
    }
    rtcmLength = i;

    String rtcmString = String(rtcmBuffer);
    
    if(timerStream) Serial.printf("taskRTCM: %0.3f\n", (millis()-rtcmTimer)/1000.0);
    timer = millis();
  }
}

// Task to send RTCM bytes
void taskLORA()
{
  if (((millis() - loraTimer) > LORA_INTERVAL) and loraStream and rtcmLength)
  {
//    Serial.printf("\nRTCM Bytes to send: %d\n\n", rtcmLength);
    // Reset timer
    loraTimer = millis();

    // Parse bytes
    parseBytes(rtcmBytes, rtcmLength);
    if (numRTCM > 2) digitalWrite(LED , HIGH);

    if (rtcmStream)
    {
      Serial.println("\nFull Message:");
      for (int i = 0; i < rtcmLength; i++)
      {
        Serial.print(rtcmBytes[i], HEX);
        Serial.print(" ");
      }
      Serial.println();
      Serial.printf("Message length: %d bytes\n\n", rtcmLength);
    }
    

    // Send each message individually over LoRa
    for (int i = 0; i < numRTCM; i++)
    {
      // Determine the length of the current message
      int currentLength;
      if (rtcmIDXs[i+1] > 0) currentLength = rtcmIDXs[i+1]-rtcmIDXs[i];
      else currentLength = rtcmLength-rtcmIDXs[i];

      int currentIndex = rtcmIDXs[i]; // Current place within rtcmBytes

      if (rtcmStream) Serial.printf("Current message length: %d\nCurrent index: %d\n\n", currentLength, currentIndex);
      
      // If the current message is small enough to send in one packet, send it
      if (currentLength <= MAX_RTCM)
      {
        if (rtcmStream) Serial.println("Writing message normally");
        
        // Declare a fresh byte array of a size we can send, reset the number of bytes
        byte rtcmMessage[MAX_RTCM];
        int index = 0;
        byte checkSum = 0;

        // Start at given index, end just before next index
        for (int idx = currentIndex; idx < (currentIndex+currentLength); idx++)
        {
          byte newByte = rtcmBytes[idx];
          rtcmMessage[index] = newByte;

          // Update checksum
          checkSum ^= newByte;
          index++;
        }
        rtcmMessage[index] = checkSum;
        index++;

        // Sends the current message
        sendLora(rtcmMessage, index);
      }

      // Otherwise, it's too big and must be sent as multiple messages
      else
      {
        if (rtcmStream) Serial.printf("Message too big: %d bytes\n", currentLength);
//        // Determine how many packets you will need
//        int numPackets = currentLength / MAX_RTCM;
//        if (currentLength % MAX_RTCM > 0) numPackets++;
//
//        if (rtcmStream) Serial.printf("Writing message in %d parts\n", numPackets);
//
//        // Split the message into as many packets as needed and send them
//        for (int packetNumber = 0; packetNumber < numPackets; packetNumber++)
//        {
//          // Calculate remaining length of portioned message
//          int remainingLength = currentLength - packetNumber*MAX_RTCM;
//
//          // Calculate starting and ending indeces
//          int endIndex = min(currentIndex+MAX_RTCM, currentIndex+remainingLength);
//          
//          if (rtcmStream) 
//          {
//            Serial.printf("Packet number %d/%d\n", packetNumber+1, numPackets);
//            Serial.printf("Bytes %d to %d\n", currentIndex, endIndex);
//          }
//          
//          // Declare a fresh byte array of a size we can send, reset the number of bytes
//          byte rtcmMessage[MAX_RTCM + 3];
//          int index = 3;
//
//          // Setup first two bytes with message information
//          rtcmMessage[0] = byte(0xa0); // Special indicator
//          rtcmMessage[1] = byte(packetNumber + 1); // Indicate which packet this is
//          rtcmMessage[2] = byte(numPackets); // Indicate the number of packets to expect
//
//          // Start at given index, end just before next index
//          for (int idx = currentIndex; idx < endIndex; idx++)
//          {
//            // Start 3 bytes after the start to account for the message information
//            rtcmMessage[index] = rtcmBytes[idx];
//            index++;
//          }
//
//          // Update start index for next pass
//          currentIndex += index - 3;
//
//          if (rtcmStream)
//          {
//            Serial.printf("Partial message %d/%d:\n", packetNumber + 1, numPackets);
////            for (int i = 0; i < index; i++)
////            {
////              Serial.print(rtcmMessage[i], HEX);
////              Serial.print(" ");
////            }
////            Serial.println();
//          }
//
//          // Send message (send first packet multiple times)
//          if (!packetNumber)
//          {
//            for (int i = 0; i < TRY_SEND; i++)
//            {
//              sendLora(rtcmMessage, index);
//            }
//          }
//
//          else sendLora(rtcmMessage, index);
//        }
      }
    }

    // Clear rtcm info for next time
    for (int i = 0; i < ARRAY_SIZE; i++)
    {
      rtcmBytes[i] = 0;
      rtcmIDXs[i] = 0;
    }

    digitalWrite(LED, LOW);

    if(timerStream) Serial.printf("taskLora: %0.3f\n", (millis()-loraTimer)/1000.0);
  }
}

void taskSD()
{
  if ((millis() - sdTimer) > SD_INTERVAL)
  {
    // Reset timer
    sdTimer = millis();

    // Check to see if the folder exists
    sdBegin();

    // Query base position
    queryBasePosition();
    
    // Create string for new file name
    String fileName = "/Data/text_";
    fileName += String(fileCounter);
    fileName += ".txt";
    fileCounter++;

    //Create and open a file
    File dataFile = SD.open(fileName, FILE_WRITE);
  
    // Write position to one line of a file
    dataFile.println("Saved Base Mode, Saved Survey Length, Standard Deviation, Saved Latitude, Saved Longitude, Saved Ellipsoidal Height, Current Base Mode, Current Survey Length");
    dataFile.print(String(savedBaseMode));
    dataFile.print(",");
    dataFile.print(String(savedSurveyLength));
    dataFile.print(",");
    dataFile.print(String(standardDeviation));
    dataFile.print(",");
    dataFile.print(String(savedLat, 7));
    dataFile.print(",");
    dataFile.print(String(savedLong, 7));
    dataFile.print(",");
    dataFile.print(String(savedAlt, 7));
    dataFile.print(",");
    dataFile.print(String(baseMode));
    dataFile.print(",");
    dataFile.print(String(surveyLength));
  
    // Close file
    dataFile.close();
  
    if(timerStream) Serial.printf("taskSD: %0.3f\n", (millis()-sdTimer)/1000.0);
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

        case 'r':
          // n: Toggle RTCM data stream
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
          if (loraStream)
          {
            // Turn stream off
            Serial.println("RTCM correction over LoRa disabled");
            loraStream = false;
          }

          else
          {
            // Turn stream on
            Serial.println("RTCM correction over LoRa enabled");
            loraStream = true;
          }
          break;

        case 'L':
          // L: Toggle LoRa printing on/off
          if (LoraBytes)
          {
            // Turn stream off
            Serial.println("LoRa printing disabled");
            LoraBytes = false;
          }

          else
          {
            // Turn stream on
            Serial.println("LoRa printing enabled");
            LoraBytes = true;
          }
          break;

        case 'T':
          // T: Toggle timers on/off
          if (timerStream)
          {
            // Turn stream off
            Serial.println("Timers disabled");
            timerStream = false;
          }

          else
          {
            // Turn stream on
            Serial.println("Timers enabled");
            timerStream = true;
          }
          break;

        case 'c':
          // c: Clear all data
          Serial.println();
          Serial.printf("Clearing %d bytes of data\n", Serial2.available());
          Serial.println();
          clearPort();
          break;

        case 'B':
          // B: Query Base Position
          Serial.println();
          Serial.println("Querying Base Position");
          Serial.println();
          queryBytes = true;
          queryBasePosition();
          queryBytes = false;
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

    if(timerStream) Serial.printf("taskUser: %0.3f\n", (millis()-userTimer)/1000.0);
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
  Serial.println("|| r: Toggle RTCM stream on/off  ||");
  Serial.println("|| l: Enable/Disable RTCM output ||");
  Serial.println("|| L: Enable/Disable LoRa print  ||");
  Serial.println("|| T: Enable/Disable task timers ||");
  Serial.println("|| c: Clear all serial data      ||");
  Serial.println("|| B: Query base position        ||");
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

    // Create header with title, timestamp, and column names
    queryBasePosition();
    
    dataFile.println("RTK Base");
    dataFile.print("Spreading Factor: ");
    dataFile.println(String(SPREAD_FACTOR));
    dataFile.print("Base Mode: ");
    dataFile.println(String(savedBaseMode));

    if (savedBaseMode == 2)
    {
      dataFile.print("  Saved Latitude: ");
      dataFile.println(String(savedLat, 7));
      dataFile.print("  Saved Longitude: ");
      dataFile.println(String(savedLong, 7));
      dataFile.print("  Saved Ellipsoidal Height [m]: ");
      dataFile.println(String(savedAlt, 7));
    }
    
    dataFile.close();
  }

  // Create data folder if not there
  if (!SD.exists("/Data"))
  {
    SD.mkdir("/Data");
  }
}

void sendLora(byte myByte[], int arraySize)
{
  if (arraySize<26) return;
  while(!LoRa.beginPacket());
  LoRa.write(myByte, arraySize); // Way faster than LoRa.print
  LoRa.endPacket(); // set true to prevent blocking code

  // Print Bytes
  if (LoraBytes)
  {
    Serial.println("\nSending Bytes:");
    for (int i = 0; i < arraySize-1; i++)
    {
      Serial.print(myByte[i], HEX);
      Serial.print(" ");
    }
    Serial.printf("\nMessage Length: %d bytes.\n\n", arraySize-1);
  }

  #ifdef LORA_DELAY
//    Serial.printf("Delay %dms\n", LORA_DELAY);
    delay(LORA_DELAY);
  #endif
}

void clearPort()
{
  Serial2.flush();
  while (Serial2.available()) Serial2.read();
}

void parseBytes(byte myByte[], int arraySize)
{
  // Reset number of rtcm messages
  numRTCM = 0;

  // Reset RTCM indexes (start of new message)
  for (int i = 0; i < 10; i++)
  {
    rtcmIDXs[i] = 0;
  }

  // Parse through all bytes from serial channel
  for (int i = 0; i < arraySize - 1; i++)
  {
    // Check groups of two bytes for D3 00
    if ((myByte[i] == 0xd3) and (myByte[i + 1] == 0x00))
    {
      // Store index of message and increment the number of RTCM messages
      rtcmIDXs[numRTCM] = i;
      numRTCM++;
      //      Serial.printf("New RTCM message found at index: %d!\n", i);
    }

  }
}

//-------------------------------------------------------------------------










//-------------------------------------------------------------------------
//----- GPS Commands ------------------------------------------------------

void queryBasePosition()
{
  byte message[] = {0xA0, 0xA1, 0x00, 0x01, 0x23, 0x23, 0x0D, 0x0A};
  Serial2.write(message, sizeof(message));
  Serial2.flush();
//  clearPort();
  while(!Serial2.available()) {}

  // Read all data from serial2 channel
  Serial2.flush();
  byte byteBuffer[Serial2.available()];
  int i = 0;
  while (Serial2.available())
  {
    byte myChar = Serial2.read();
    byteBuffer[i] = myChar;
    i++;
  }

  // Message was recieved succesfully
  if (byteBuffer[4] == 0x83)
  {
    if (queryBytes)
    {
      Serial.println("Queried Base Position:");
      for (int idx = 0; idx < 9; idx++)
      {
        Serial.print(byteBuffer[idx], HEX);
        Serial.print(" ");
      }
      Serial.println("\n");
        
      for (int idx = 9; idx < sizeof(byteBuffer); idx++)
      {
        Serial.print(byteBuffer[idx], HEX);
        Serial.print(" ");
      }
      Serial.println("\n");
    }

    //-------------------------------------------------------------
    
    // Update saved survey length (3-6)
    byte savedSurveyBytes[4];
    for (uint8_t idx = 0; idx < 4; idx++)
    {
      savedSurveyBytes[3-idx] = byteBuffer[idx+3 + 3+9];
    }

    // Update standard deviation (7-10)
    byte standardDeviationBytes[4];
    for (uint8_t idx = 0; idx < 4; idx++)
    {
      standardDeviationBytes[3-idx] = byteBuffer[idx+7 + 3+9];
    }

    // Update latitude and longitude (11-18 | 19-26)
    byte savedLatBytes[8];
    byte savedLongBytes[8];
    for (uint8_t idx = 0; idx < 8; idx++)
    {
      savedLatBytes[7-idx] = byteBuffer[idx+11 + 3+9];
      savedLongBytes[7-idx] = byteBuffer[idx+19 + 3+9];
    }

    // Update altitude (27-30)
    byte savedAltBytes[4];
    for (uint8_t idx = 0; idx < 4; idx++)
    {
      savedAltBytes[3-idx] = byteBuffer[idx+27 + 3+9];
    }

    // Update current survey length (32-35)
    byte surveyBytes[4];
    for (uint8_t idx = 0; idx < 4; idx++)
    {
      surveyBytes[3-idx] = byteBuffer[idx+32 + 3+9];
    }


    savedBaseMode = byteBuffer[2 + 3+9];
    memcpy(&savedSurveyLength, savedSurveyBytes, 4);
    memcpy(&savedLat, savedLatBytes, 8);
    memcpy(&savedLong, savedLongBytes, 8);
    memcpy(&savedAlt, savedAltBytes, 4);
    baseMode = byteBuffer[31 + 3+9];
    memcpy(&surveyLength, surveyBytes, 4);
    

    if (queryBytes)
    {
      Serial.printf("Saved Base Mode: %d\n", savedBaseMode);
      Serial.printf("Saved Survey Length: %d\n", savedSurveyLength);
      Serial.printf("Standard Deviation: %d\n", standardDeviation);
      Serial.printf("Saved Latitude: %f\n", savedLat);
      Serial.printf("Survey Longitude: %f\n", savedLong);
      Serial.printf("Survey Altitude: %f\n", savedAlt);
      Serial.printf("Current Base Mode: %d\n", baseMode);
      Serial.printf("Current Survey Length: %d\n", surveyLength);
    }
    
    //-------------------------------------------------------------
  }
  
  else
  {
    if (queryBytes) Serial.println("Unable to interpret command");

//    for (int idx = 0; idx < sizeof(byteBuffer); idx++)
//    {
//      Serial.print(byteBuffer[idx], HEX);
//      Serial.print(" ");
//    }
//    Serial.println("\n");
  }
}

//-------------------------------------------------------------------------
