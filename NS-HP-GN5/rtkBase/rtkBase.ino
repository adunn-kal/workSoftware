// RTK Base using Sparkfun GPS
// 07/18/22

// SDA: 21
// SCL: 22

// MISO: 19
// MOSI: 23
// SCLK: 18

#include <Wire.h> //Needed for I2C to GNSS
#include <SparkFun_u-blox_GNSS_Arduino_Library.h> //http://librarymanager/All#SparkFun_u-blox_GNSS
#include <SPI.h>
#include <LoRa.h>

// Define task info
#define UBLOX_INTERVAL 250 // ms
#define USER_INTERVAL 250

// Define the pins used by the transceiver module
#define ss 5
#define rst 14
#define dio0 4
#define LED 2

// Define constants
#define ARRAY_SIZE 500 // Maximum # of bytes to send at once
#define minTime 30 // Minimum survey time [s]            (Recommended: 300)
#define minAccuracy 10.0 // Minimum survey accuracy [m]  (Recommended: 2.0)

SFE_UBLOX_GNSS myGNSS;
int messageLength;
byte sendArray[ARRAY_SIZE];

bool surveyFinished = false;
bool rtcmStream = true;
bool loraStream = true;

long ubloxTimer;
long userTimer;

//------------------------------------------------------------------------

void setup()
{
  Serial.begin(115200);

  // Setup LoRa transceiver module
  LoRa.setPins(ss, rst, dio0);
  pinMode(LED, OUTPUT);
  
  //replace the LoRa.begin(---E-) argument with your location's frequency 
  //433E6 for Asia
  //866E6 for Europe
  //915E6 for North America
  while (!LoRa.begin(915E6)) {
    Serial.println(".");
    delay(500);
  }
  
  // Change sync word (0xF3) to match the receiver
  // The sync word assures you don't get LoRa messages from other LoRa transceivers
  // ranges from 0-0xFF
  LoRa.setSyncWord(0xF3);
  Serial.println("LoRa Initializing OK!");

  Wire.begin();
  Wire.setClock(400000); //Increase I2C clock speed to 400kHz

  if (myGNSS.begin() == false) //Connect to the u-blox module using Wire port
  {
    Serial.println(F("u-blox GNSS not detected at default I2C address. Please check wiring. Freezing."));
    while (1);
  }

  myGNSS.setI2COutput(COM_TYPE_UBX | COM_TYPE_RTCM3); //Set the I2C port to output UBX only (turn off NMEA noise)
  myGNSS.saveConfiguration(); //Save the current settings to flash and BBR

  while (Serial.available()) Serial.read(); //Clear any latent chars in serial buffer

  bool response;

 response = myGNSS.getSurveyStatus(2000); //Query module for SVIN status with 2000ms timeout (request can take a long time)
  
  if (response == false) // Check if fresh data was received
  {
    Serial.println(F("Failed to get Survey In status. Freezing..."));
    while (1); //Freeze
  }

  // Check to see if a survey has already been started because this is gonna make stuff go screwy
  if (myGNSS.getSurveyInActive() == true) // Disable existing survey
  {
    response = myGNSS.disableSurveyMode(); //Disable survey
  }

  if (myGNSS.getSurveyInActive() == true) // Use the helper function
  {
    Serial.print(F("Survey already in progress."));
  }
  else
  {
    //Start survey                      Time     Accuracy
    response = myGNSS.enableSurveyMode(minTime, minAccuracy); //Enable Survey in
    if (response == false)
    {
      Serial.println(F("Survey start failed. Freezing..."));
      while (1);
    }
    Serial.printf("Survey started. This will run until %ds has passed and less than %.1fm accuracy is achieved.\n", minTime, minAccuracy);
  }

  while (Serial.available()) Serial.read(); //Clear buffer
  
  //Begin waiting for survey to complete
  while (myGNSS.getSurveyInValid() == false) // Call the helper function
  {
    if (Serial.available())
    {
      byte incoming = Serial.read();
      if (incoming == 'x')
      {
        //Stop survey mode
        response = myGNSS.disableSurveyMode(); //Disable survey
        Serial.println("Survey stopped");
        break;
      }
    }

//    // Stop early
//    if (myGNSS.getSurveyInMeanAccuracy() < minAccuracy)
//    {
//      // Stop survey mode
//      response = myGNSS.disableSurveyMode(); //Disable survey
//      Serial.println("Minimum accuracy acheived, survey stopped");
//      break;
//    }
    
    response = myGNSS.getSurveyStatus(2000); //Query module for SVIN status with 2000ms timeout (req can take a long time)
    if (response == true) // Check if fresh data was received
    {
      Serial.print(F("Press x to end survey - "));
      Serial.print(F("Time elapsed: "));
      Serial.print((String)myGNSS.getSurveyInObservationTime());

      Serial.print(F(" Accuracy: "));
      Serial.print((String)myGNSS.getSurveyInMeanAccuracy());
      Serial.println();
    }
    else
    {
      Serial.println(F("SVIN request failed"));
    }

    delay(1000);
  }
  Serial.println(F("Survey valid!"));

  response = true;

//---------------------------------------------------------------------------------------------||
  // Enable RTCM messages                                                                      ||
  response &= myGNSS.enableRTCMmessage(UBX_RTCM_1005, COM_PORT_I2C, 1); //Base Station ARP     ||
  response &= myGNSS.enableRTCMmessage(UBX_RTCM_1074, COM_PORT_I2C, 1); //GPS MSM4             ||
  response &= myGNSS.enableRTCMmessage(UBX_RTCM_1077, COM_PORT_I2C, 1); //GPS MSM7             ||
//  response &= myGNSS.enableRTCMmessage(UBX_RTCM_1084, COM_PORT_I2C, 1); //GLONASS MSM4         ||
//  response &= myGNSS.enableRTCMmessage(UBX_RTCM_1087, COM_PORT_I2C, 1); //GLONASS MSM7         ||
//  response &= myGNSS.enableRTCMmessage(UBX_RTCM_1124, COM_PORT_I2C, 1); //BeiDou MSM4          ||
//  response &= myGNSS.enableRTCMmessage(UBX_RTCM_1127, COM_PORT_I2C, 1); //BeiDou MSM7          ||
                                                                                             //||
  // Disable RTCM messages                                                                     ||
//  response &= myGNSS.disableRTCMmessage(UBX_RTCM_1005, COM_PORT_I2C); //Base Station ARP     ||
//  response &= myGNSS.disableRTCMmessage(UBX_RTCM_1074, COM_PORT_I2C); //GPS MSM4             ||
//  response &= myGNSS.disableRTCMmessage(UBX_RTCM_1077, COM_PORT_I2C); //GPS MSM7               ||
  response &= myGNSS.disableRTCMmessage(UBX_RTCM_1084, COM_PORT_I2C); //GLONASS MSM4           ||
  response &= myGNSS.disableRTCMmessage(UBX_RTCM_1087, COM_PORT_I2C); //GLONASS MSM7           ||
  response &= myGNSS.disableRTCMmessage(UBX_RTCM_1124, COM_PORT_I2C); //BeiDou MSM4            ||
  response &= myGNSS.disableRTCMmessage(UBX_RTCM_1127, COM_PORT_I2C); //BeiDou MSM7            ||

  response &= myGNSS.disableRTCMmessage(UBX_RTCM_1230, COM_PORT_I2C); //Extra data?            ||
  response &= myGNSS.disableRTCMmessage(UBX_RTCM_4072_0, COM_PORT_I2C); //Reference PVT        ||
//---------------------------------------------------------------------------------------------||

  // Verify that everyhtning enabled correctly
  if (response == true)
  {
    Serial.println(F("RTCM messages enabled"));
  }
  else
  {
    Serial.println(F("RTCM failed to enable. Are you sure you have an NEO-M8P?"));
    while (1); //Freeze
  }

  // Print verification message and proceed
  Serial.println("Base survey complete! RTCM now broadcasting.");
  digitalWrite(LED, HIGH);
  surveyFinished = true;
  printHelp();
}

void loop()
{
  taskUblox();
  taskUser();
}

//------------------------------------------------------------------------
// Functions

void SFE_UBLOX_GNSS::processRTCM(uint8_t incoming)
{
  if (surveyFinished)
  {
    if (myGNSS.rtcmFrameCounter == 1)
    {
      messageLength = myGNSS.rtcmLen;
    }
    
    // Add incoming bytes to array
    sendArray[myGNSS.rtcmFrameCounter - 1] = incoming;
    
    // If all bytes have been added to array
    if (myGNSS.rtcmFrameCounter == myGNSS.rtcmLen)
    {
      // Send array over LoRa
      if (loraStream)
      {
        byte loraArray[messageLength];
        for (int i = 0; i < messageLength; i++)
        {
//          Serial.print(sendArray[i], HEX);
//          Serial.print(" ");

          // Add byte to loraArray
          loraArray[i] = sendArray[i];

          // Clear byte from 
          sendArray[i] = 0x00;
        }
        sendLora(loraArray, messageLength);
//        Serial.println();
      }
    }

    if(rtcmStream)
    {
      Serial.print(F(" "));
      if (incoming < 0x10) Serial.print(F("0"));
      Serial.print(incoming, HEX);
      if (myGNSS.rtcmFrameCounter == myGNSS.rtcmLen)
      {
        Serial.println();
        Serial.printf("Message length: %d bytes\n", myGNSS.rtcmLen);
        Serial.println();
      }
    }
  }
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

void printHelp()
{
  Serial.println();
  Serial.println("||------------COMMANDS-----------||");
  Serial.println("|| h: Print this help message    ||");
  Serial.println("|| r: Toggle RTCM stream on/off  ||");
  Serial.println("|| l: Toggle LORA stream on/off  ||");
  Serial.println("|| a: Print GPS accuracy         ||");
  Serial.println("|| s: Print satellites in view   ||");
  Serial.println("|| f: Print fix type             ||");
  Serial.println("|| p: Print current position     ||");
  Serial.println("||-------------------------------||");
  Serial.println();
}


//------------------------------------------------------------------------
// Tasks

void taskUblox()
{
  if ((millis() - ubloxTimer) > UBLOX_INTERVAL)
  {
    // Reset timer
    ubloxTimer = millis();
    
    myGNSS.checkUblox();
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
          // l: Toggle LORA data stream
          if (loraStream)
          {
            // Turn stream off
            Serial.println("LORA data stream turned off");
            loraStream = false;
          }

          else
          {
            // Turn stream on
            Serial.println("LORA data stream turned on");
            loraStream = true;
          }
          break;

        case 'a':
          // a: Print GPS accuracy
          Serial.println();
          Serial.println("Querying GPS accuracy...");
          Serial.printf("GPS Accuracy: %.2f m\n", myGNSS.getPositionAccuracy()/1000.0);
          Serial.println();
          break;

        case 's':
          // s: Print satellites in view
          Serial.println();
          Serial.println("Querying satellites in view...");
          Serial.printf("Satellites in view: %d\n", myGNSS.getSIV());
          Serial.println();
          break;

        case 'f':
          // f: Print fix type
          Serial.println();
          Serial.println("Querying fix type...");
          
          Serial.print("GPS Fix Type: "); // 0=no fix, 1=dead reckoning, 2=2D, 3=3D, 4=GNSS, 5=Time fix
          switch (myGNSS.getFixType())
          {
            case 0:
              Serial.println("No fix");
              break;

            case 1:
              Serial.println("Dead reckoning");
              break;

            case 2:
              Serial.println("2D fix");
              break;

            case 3:
              Serial.println("3D fix");
              break;

            case 4:
              Serial.println("GNSS fix");
              break;

            case 5:
              Serial.println("Time fix");
              break;

            default:
              Serial.println("Error");
              break;
          }
          
          Serial.print("RTK Solution Type: "); // 0=No solution, 1=Float solution, 2=Fixed solution
          switch (myGNSS.getCarrierSolutionType())
          {
            case 0:
              Serial.println("No solution");
              break;

            case 1:
              Serial.println("Floating solution");
              break;

            case 2:
              Serial.println("Fixed solution");
              break;

            default:
              Serial.println("Error");
              break;
          }
          
          Serial.println();
          break;

        case 'p':
          // p: Print position
          Serial.println();
          Serial.println("Querying postiton...");
          Serial.println("Latitude, Longitude, Altitude");
          Serial.printf("%f, %f, %f\n", float(myGNSS.getLatitude()/pow(10, 7)), float(myGNSS.getLongitude()/pow(10, 7)), float(myGNSS.getAltitude()/1000.0));
          Serial.println();
          break;

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
