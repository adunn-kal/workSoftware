// Serial Reader
// 09/10/22

// Input for reading serial
#define RX1 14
#define TX1 32 // Not actually used


//-------------------------------------------------------------

void setup()
{
  Serial.begin(115200);
  Serial1.begin(115200, SERIAL_8N1, RX1, TX1);

  Serial.println("Listening");
}

void loop()
{
  while (!Serial1.available()); // Wait until data is available

  parseChars();
}


//-------------------------------------------------------------

void parseChars()
{
  // Read all data from serial1 channel
  Serial1.flush();
  Serial.println();
  Serial.println("Data:");
  byte byteBuffer[Serial1.available()];
  char charBuffer[Serial1.available()];
  int i = 0;
  while (Serial1.available())
  {
    byte myChar = Serial1.read();
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
  Serial.printf("Message Length: %d bytes \n", sizeof(byteBuffer));
}

void parseBytes()
{
  // Read all data from serial1 channel
  Serial1.flush();
  Serial.println();
  Serial.println("Changing to RTK survey mode:");
  byte byteBuffer[Serial1.available()];
  int i = 0;
  while (Serial1.available())
  {
    byte myChar = Serial1.read();
    byteBuffer[i] = myChar;
    i++;
  }

  bool received = printBuffer(byteBuffer, sizeof(byteBuffer));
}

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
