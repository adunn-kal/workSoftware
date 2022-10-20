// Serial Mirror
// 09/12/22

// Pins
#define RX1 16
#define TX1 17

// Task Intervals
#define TX_INTERVAL 0
#define RX_INTERVAL 0

long txTimer;
long rxTimer;

void setup()
{
  Serial.begin(115200);
  Serial1.begin(115200, SERIAL_8N1, RX1, TX1);
}

void loop()
{
  taskTX();

  taskRX();
}


void taskTX()
{
  if ((millis() - txTimer) > TX_INTERVAL)
  {
    while (Serial1.available())
    {
      Serial1.write(Serial1.read());
    }
  }
}

void taskRX()
{
  if ((millis() - rxTimer) > RX_INTERVAL)
  {
    while (Serial1.available())
    {
      Serial1.write(Serial1.read());
    }
  }
}
