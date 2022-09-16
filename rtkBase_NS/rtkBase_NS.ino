// RTK Base using NS-HP-GN5
// 09/16/22
// https://navspark.mybigcommerce.com/ns-hp-gn5-px1125r-l1-l5-rtk-breakout-board/ 

#include <SPI.h>
#include <LoRa.h>
#include <Wire.h>
#include <SD.h>

#define RTCM_INTERVAL 1000 // ms between RTCM updates
#define LORA_INTERVAL 1000 // ms between NMEA updates
#define USER_INTERVAL 200 // ms between user interaction

// Define Pins and Constants
#define LED 2
#define TRIG 13

// Define the pins used by the transceiver module
#define ss 5
#define rst 4
#define dio0 4

// Talk to GPS module, get RTCM
#define RX2 16
#define TX2 17

long rtcmTimer;
long loraTimer;
long userTimer;

byte rtcmBytes[1000];
int rtcmLength;

bool rtcmStream = false;
bool loraStream = true;

void setup()
{
  // Start computer-ESP serial
  Serial.begin(115200);

  // Start ESP-GPS serial
  Serial2.begin(115200, SERIAL_8N1, RX2, TX2);

  // Set pins
  pinMode(LED, OUTPUT);
  pinMode(TRIG, OUTPUT);
  pinMode(TRIG, LOW);

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
  Serial.println("Starting!\n");
  Serial1.flush();
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
}

//------------------------------------------------------------
// Tasks

// Task to send RTCM bytes
void taskLORA()
{
  if (((millis() - loraTimer) > LORA_INTERVAL) and loraStream)
  {
    // Reset timer
    loraTimer = millis();

    // Send data over LoRa
    sendLora(rtcmBytes, rtcmLength);

    // Clear rtcmBytes for next time
    Serial.println("Sending Bytes:");
    for (int i = 0; i < rtcmLength; i++)
    {
      Serial.print(rtcmBytes[i], HEX);
      Serial.print(" ");
      rtcmBytes[i] = 0;
    }
    Serial.println();
  }
}

void taskRTCM()
{
  // Display RTCM readings at set RTCM_INTERVAL
  if ((millis() - rtcmTimer) > RTCM_INTERVAL)
  { 
    digitalWrite(LED, HIGH);
    // Reset timer
    rtcmTimer = millis();
      
    // Read all data from serial2 channel
    char rtcmBuffer[1000];
    int i = 0;
    Serial2.flush();
    while (Serial2.available())
    {
      char myChar = Serial2.read();
      rtcmBuffer[i] = myChar;
      rtcmBytes[i] = byte(myChar);
      i++;
//      Serial.print(byte(myChar), HEX);
//      Serial.print(" ");
    }
    rtcmLength = i;
  
    String rtcmString = String(rtcmBuffer);
    
    if (rtcmStream)
    {
      Serial.println();
      Serial.println(rtcmString);
      Serial.println();
    }
    digitalWrite(LED, LOW);
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

        case 'c':
          // c: Clear all data
          Serial.println();
          Serial.printf("Clearing %d bytes of data\n", Serial2.available());
          Serial.println();
          clearPort();
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
  }
}

//-----------------------------------------------------------------------
// Functions

void printHelp()
{
  Serial.println();
  Serial.println("||------------COMMANDS-----------||");
  Serial.println("|| h: Print this help message    ||");
  Serial.println("|| r: Toggle RTCM stream on/off  ||");
  Serial.println("|| l: Enable/Disable RTCM output ||");
  Serial.println("|| c: Clear all serial data      ||");
  Serial.println("||-------------------------------||");
  Serial.println();
}

void sendLora(byte myByte[], int arraySize)
{
  LoRa.beginPacket();
  
  String myString;
  for (int i = 0; i < arraySize; i++)
  {
    myString += String(myByte[i], HEX);
    myString += " ";
  }
  LoRa.print(myString);
//  Serial.println(myString);
  
//  LoRa.print(myByte);
  LoRa.endPacket();
}

void clearPort()
{
  Serial2.flush();
  while (Serial2.available())
  {
    Serial2.read();
  }
}
