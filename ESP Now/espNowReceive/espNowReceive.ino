#include <esp_now.h>
#include <WiFi.h>

//Structure example to receive data
//Must match the sender structure
typedef struct test_struct
{
  time_t timeStamp;
  int distance;
  float temp;
  float humidity;
} test_struct;

//Create a struct_message called myData
test_struct myData;

//callback function that will be executed when data is received
void OnDataRecv(const uint8_t * mac, const uint8_t *incomingData, int len)
{
  memcpy(&myData, incomingData, sizeof(myData));
  
  Serial.printf("Bytes received: %d\n", len);
  Serial.printf("Time Stamp: %d\n", myData.timeStamp);
  Serial.printf("Distance: %dmm\n", myData.distance);
  Serial.printf("Temperature [F]: %f\n", myData.temp);
  Serial.printf("Humidity [%]: %f\n", myData.humidity);
  
}
 
void setup()
{
  //Initialize Serial Monitor
  Serial.begin(115200);
  
  //Set device as a Wi-Fi Station
  WiFi.mode(WIFI_STA);

  //Init ESP-NOW
  if (esp_now_init() != ESP_OK)
  {
    Serial.println("Error initializing ESP-NOW");
    return;
  }
  
  // Once ESPNow is successfully Init, we will register for recv CB to
  // get recv packer info
  esp_now_register_recv_cb(OnDataRecv);
}
 
void loop()
{

}
