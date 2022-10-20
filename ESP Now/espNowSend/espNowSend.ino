#include <esp_now.h>
#include <WiFi.h>

// REPLACE WITH YOUR ESP RECEIVER'S MAC ADDRESS
uint8_t broadcastAddress1[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

typedef struct test_struct
{
  time_t timeStamp;
  int distance;
  float temp;
  float humidity;
} test_struct;

test_struct sendData;

esp_now_peer_info_t peerInfo;

// callback when data is sent
void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  char macStr[18];
  Serial.print("Packet to: ");
  // Copies the sender mac address to a string
  snprintf(macStr, sizeof(macStr), "%02x:%02x:%02x:%02x:%02x:%02x",
           mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
  Serial.print(macStr);
  Serial.print(" send status:\t");
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Delivery Success" : "Delivery Fail");
}
 
void setup() {
  Serial.begin(115200);
 
  WiFi.mode(WIFI_STA);
 
  if (esp_now_init() != ESP_OK)
  {
    Serial.println("Error initializing ESP-NOW");
    return;
  }
  
  esp_now_register_send_cb(OnDataSent);
   
  // register peer
  peerInfo.channel = 0;  
  peerInfo.encrypt = false;
  
  // register first peer  
  memcpy(peerInfo.peer_addr, broadcastAddress1, 6);
  
  if (esp_now_add_peer(&peerInfo) != ESP_OK)
  {
    Serial.println("Failed to add peer");
    return;
  }
  
}
 
void loop()
{
  sendData.timeStamp = 1666223877;
  sendData.distance = 9999;
  sendData.temp = 77.51;
  sendData.humidity = 62.36;
 
  esp_err_t result = esp_now_send(0, (uint8_t *) &sendData, sizeof(test_struct));
   
  if (result == ESP_OK)
  {
    Serial.println("Sent with success");
  }
  
  else
  {
    Serial.println("Error sending the data");
  }
  
  delay(2000);
}
