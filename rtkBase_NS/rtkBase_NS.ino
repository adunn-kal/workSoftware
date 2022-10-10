// RTK Base using NS-HP-GN5
// 09/16/22
// https://navspark.mybigcommerce.com/ns-hp-gn5-px1125r-l1-l5-rtk-breakout-board/

#include <SPI.h>
#include <LoRa.h>
#include <Wire.h>
#include <SD.h>

#define RTCM_INTERVAL 50 // ms between RTCM updates
#define LORA_INTERVAL 50 // ms between NMEA updates
#define USER_INTERVAL 500 // ms between user interaction
#define MAX_RTCM 70
#define ARRAY_SIZE 1000

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

byte rtcmBytes[ARRAY_SIZE];
int rtcmLength;

int numRTCM;
int rtcmIDXs[ARRAY_SIZE];

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
  if (((millis() - loraTimer) > LORA_INTERVAL) and loraStream and rtcmLength)
  {
    // Reset timer
    loraTimer = millis();

    // Parse bytes
    parseBytes(rtcmBytes, rtcmLength);
    if (numRTCM > 2) digitalWrite(LED , HIGH);

    Serial.println("\nFull Message:");
    for (int i = 0; i < rtcmLength; i++)
    {
      Serial.print(rtcmBytes[i], HEX);
      Serial.print(" ");
    }
    Serial.println();
    Serial.printf("Message length: %d bytes\n\n", rtcmLength);





    // Send each message individually over LoRa
    for (int i = 0; i < numRTCM; i++)
    {
      // Determine the length of the current message
      int currentLength;
      if (rtcmIDXs[i+1] > 0) currentLength = rtcmIDXs[i+1]-rtcmIDXs[i];
      else currentLength = rtcmLength-rtcmIDXs[i];

      int currentIndex = rtcmIDXs[i]; // Current place within rtcmBytes

      Serial.printf("Current message length: %d\nCurrent index: %d\n\n", currentLength, currentIndex);
      
      // If the current message is small enough to send in one packet, send it
      if (currentLength <= MAX_RTCM)
      {
        Serial.println("Writing message normally");
        
        // Declare a fresh byte array of a size we can send, reset the number of bytes
        byte rtcmMessage[MAX_RTCM];
        int index = 0;

        // Start at given index, end just before next index
        for (int idx = currentIndex; idx < (currentIndex+currentLength); idx++)
        {
          rtcmMessage[index] = rtcmBytes[idx];
          index++;
        }

        // Sends the current message
        sendLora(rtcmMessage, index);
      }

      // Otherwise, it's too big and must be sent as multiple messages
      else
      {
        // Determine how many packets you will need
        int numPackets = currentLength / MAX_RTCM;
        if (currentLength % MAX_RTCM > 0) numPackets++;

        Serial.printf("Writing message in %d parts\n", numPackets);

        // Split the message into as many packets as needed and send them
        for (int packetNumber = 0; packetNumber < numPackets; packetNumber++)
        {
          // Calculate remaining length of portioned message
          int remainingLength = currentLength - packetNumber*MAX_RTCM;

          // Calculate starting and ending indeces
          int endIndex = min(currentIndex+MAX_RTCM, currentIndex+remainingLength);
//          int endIndex = currentIndex + currentLength;

          
          Serial.printf("Packet number %d/%d\n", packetNumber+1, numPackets);
          Serial.printf("Bytes %d to %d\n", currentIndex, endIndex);
          
          // Declare a fresh byte array of a size we can send, reset the number of bytes
          byte rtcmMessage[MAX_RTCM + 3];
          int index = 3;

          // Setup first two bytes with message information
          rtcmMessage[0] = byte(0xa0); // Special indicator
          rtcmMessage[1] = byte(packetNumber + 1); // Indicate which packet this is
          rtcmMessage[2] = byte(numPackets); // Indicate the number of packets to expect

          // Start at given index, end just before next index
          for (int idx = currentIndex; idx < endIndex; idx++)
          {
            // Start 3 bytes after the start to account for the message information
            rtcmMessage[index] = rtcmBytes[idx];
            index++;
          }

          // Update start index for next pass
          currentIndex += index - 3;

          Serial.printf("Partial message %d/%d:\n", packetNumber + 1, numPackets);
//          for (int i = 0; i < index; i++)
//          {
//            Serial.print(rtcmMessage[i], HEX);
//            Serial.print(" ");
//          }
//          Serial.println();

          // Send message
          for (int i = 0; i < 1; i++)
          {
            sendLora(rtcmMessage, index);
          }
        }
      }
    }

    // Clear rtcm info for next time
    for (int i = 0; i < ARRAY_SIZE; i++)
    {
      rtcmBytes[i] = 0;
      rtcmIDXs[i] = 0;
    }

    digitalWrite(LED, LOW);
  }
}



//      // If the message is too long to send in one packet
//      //-----------------------------------------------------------------------------
//      if (min(rtcmLength, (rtcmIDXs[i+1]-rtcmIDXs[i])) > MAX_RTCM)
//      {
//        if (rtcmIDXs[i+1] > 0)
//        {
//          if ((rtcmIDXs[i+1]-rtcmIDXs[i]) > MAX_RTCM)
//          {
//            int currentLength = rtcmIDXs[i+1]-rtcmIDXs[i];
//            
//            // Determine how many packets you will need
//            int numPackets = currentLength / MAX_RTCM;
//            if (currentLength % MAX_RTCM > 0) numPackets++;
//  
//            // Calculate strating and ending indeces
//            int startIDX = rtcmIDXs[i];
//            int endIDX = currentLength;
//  
//            // Split the message into as many packets as needed and send them
//            for (int packetNumber = 0; packetNumber < numPackets; packetNumber++)
//            {
//              // Declare a fresh byte array of a size we can send, reset the number of bytes
//              byte rtcmMessage[MAX_RTCM + 3];
//              int index = 3;
//  
//              // Setup first two bytes with message information
//              rtcmMessage[0] = byte(160); // Special indicator
//              rtcmMessage[1] = byte(packetNumber + 1); // Indicate which packet this is
//              rtcmMessage[2] = byte(numPackets); // Indicate the number of packets to expect
//  
//              // Determine ending index for this packet
//              int currentEndIDX = min(endIDX, startIDX + MAX_RTCM);
//  
//              // Start at given index, end just before next index
//              for (int idx = startIDX; idx < currentEndIDX; idx++)
//              {
//                // Start 3 bytes after the start to account for the message information
//                rtcmMessage[index] = rtcmBytes[idx];
//                index++;
//              }
//  
//              // Update start index for next pass
//              startIDX += index - 3;
//  
//              Serial.printf("Partial message %d/%d:\n", packetNumber + 1, numPackets);
//              for (int i = 0; i < sizeof(rtcmMessage); i++)
//              {
//                Serial.print(rtcmMessage[i], HEX);
//                Serial.print(" ");
//              }
//              Serial.println();
//  
//              // Send message
//              for (int i = 0; i < 2; i++)
//              {
//                sendLora(rtcmMessage, index);
//              }
//            }
//          }
//        }
//  
//        // If there's only 1 message
//        else
//        {
//          if (rtcmLength > MAX_RTCM)
//          {
//            // Determine how many packets you will need
//            int numPackets = rtcmLength / MAX_RTCM;
//            if (rtcmLength % MAX_RTCM > 0) numPackets++;
//  
//            // Calculate strating and ending indeces
//            int startIDX = rtcmIDXs[i];
//            int endIDX = rtcmLength;
//  
//            // Split the message into as many packets as needed and send them
//            for (int packetNumber = 0; packetNumber < numPackets; packetNumber++)
//            {
//              // Declare a fresh byte array of a size we can send, reset the number of bytes
//              byte rtcmMessage[MAX_RTCM + 3];
//              int index = 3;
//  
//              // Setup first two bytes with message information
//              rtcmMessage[0] = byte(160); // Special indicator
//              rtcmMessage[1] = byte(packetNumber + 1); // Indicate which packet this is
//              rtcmMessage[2] = byte(numPackets); // Indicate the number of packets to expect
//  
//              // Determine ending index for this packet
//              int currentEndIDX = min(endIDX, startIDX + MAX_RTCM);
//  
//              // Start at given index, end just before next index
//              for (int idx = startIDX; idx < currentEndIDX; idx++)
//              {
//                // Start 3 bytes after the start to account for the message information
//                rtcmMessage[index] = rtcmBytes[idx];
//                index++;
//              }
//  
//              // Update start index for next pass
//              startIDX += index - 3;
//  
//              Serial.printf("Partial message %d/%d:\n", packetNumber + 1, numPackets);
//              for (int i = 0; i < sizeof(rtcmMessage); i++)
//              {
//                Serial.print(rtcmMessage[i], HEX);
//                Serial.print(" ");
//              }
//              Serial.println();
//  
//              // Send message
//              for (int i = 0; i < 2; i++)
//              {
//                sendLora(rtcmMessage, index);
//              }
//            }
//          }
//        }
//      }
//
//      //------------------------------------------------------------------------------
//      // Otherwise, the message is short enough to send in one packet
//      else
//      {
//        // Declare a fresh byte array of a size we can send, reset the number of bytes
//        byte rtcmMessage[MAX_RTCM];
//        int index = 0;
//
//        // Check to see if there is another message after this one
//        if (rtcmIDXs[i + 1] > 0)
//        {
//          // Start at given index, end just before next index
//          for (int idx = rtcmIDXs[i]; idx < rtcmIDXs[i + 1]; idx++)
//          {
//            rtcmMessage[index] = rtcmBytes[idx];
//            index++;
//          }
//        }
//
//        // Otherwise, this is the last message
//        else
//        {
//          // Start at given index, end after the message length
//          for (int idx = rtcmIDXs[i]; idx < rtcmLength; idx++)
//          {
//            rtcmMessage[index] = rtcmBytes[idx];
//            index++;
//          }
//        }
//
//        // Sends the current message
//        sendLora(rtcmMessage, index);
//      }
//      //------------------------------------------------------------------------------
//
//    }
//
//    //    sendLora(rtcmBytes, rtcmLength);
//
//    // Clear rtcmBytes for next time
//    for (int i = 0; i < rtcmLength + 1; i++)
//    {
//      rtcmBytes[i] = 0;
//    }
//
//    digitalWrite(LED, LOW);
//  }
//}

void taskRTCM()
{
  // Display RTCM readings at set RTCM_INTERVAL
  if ((millis() - rtcmTimer) > RTCM_INTERVAL)
  {
    // Reset timer
    rtcmTimer = millis();

    // Read all data from serial2 channel
    char rtcmBuffer[1000];
    int i = 0;
    Serial2.flush();
    while (Serial2.available())
    {
      byte myByte = Serial2.read();
      rtcmBuffer[i] = char(myByte);
      rtcmBytes[i] = myByte;
      i++;
    }
    rtcmLength = i;

    String rtcmString = String(rtcmBuffer);

    if (rtcmStream)
    {
      Serial.println();
      Serial.println(rtcmString);
      Serial.println();
    }
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
  LoRa.endPacket();

  // Print Bytes
  Serial.println("Sending Bytes:");
  for (int i = 0; i < arraySize; i++)
  {
    Serial.print(myByte[i], HEX);
    Serial.print(" ");
  }
  Serial.printf("\nMessage Length: %d bytes.\n\n", arraySize);
}

void clearPort()
{
  Serial2.flush();
  while (Serial2.available())
  {
    Serial2.read();
  }
}

void parseBytes(byte myByte[], int arraySize)
{
  // Reset number of rtcm messages
  numRTCM = 0;

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
