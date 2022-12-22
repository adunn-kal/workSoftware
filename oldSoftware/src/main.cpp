/**
 * @file main.cpp
 * @author Alexander Dunn
 * @author Caleb Dennis
 * @version 0.1
 * @date 2022-12-14
 * 
 * @copyright Copyright (c) 2022
 * 
 */

#include <Arduino.h>
#include <utility>
#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include <driver/timer.h>
#include <soc/rtc.h>
#include "Adafruit_SHT31.h"
#include "Adafruit_GPS.h"
#include "UnixTime.h"
#include <esp_now.h>
#include <WiFi.h>

//-----------------------------------------------------------------------------------------------------||
//---------- Configuration ----------------------------------------------------------------------------||

#define DEBUG // Comment out this line if monitoring over Serial is not desired

//#define TRANSMIT // Comment out this line if you do not wih to transmit data over espNow

/* In non-continuous measurement mode measurements will be READ_TIME long and
 * centered on mulitples of MINUTE_ALLIGN value after the hour
 */
#define READ_TIME 20 // Length of time to measure (in seconds)
#define MINUTE_ALLIGN 2 // Minutes

/* In continuous measurement mode the device never goes to sleep,
 * data is stored in the Data folder, a new file is created every READ_TIME seconds
 */
//#define CONTINUOUS // Comment out this line if non-continuous measurement is desired

//-----------------------------------------------------------------------------------------------------||










//-----------------------------------------------------------------------------------------------------||
//---------- Define Constants -------------------------------------------------------------------------||

#define MEASUREMENT_HZ 5.64 //MB 7388 (10 meter sensor)
//#define EFF_HZ 6.766 //MB 7388 (5 meter sensor)

#define celsius_to_fahrenheit(__celsius) ((__celsius) * 9.0 / 5.0 + 32.0)
#define GMT_to_PST(__GMT) (((__GMT) + 16) % 24)

#define FORMAT_BUF_SIZE 200//(bytes) if this is too small some data lines may be truncated

#define FIX_DELAY 60 // Seconds to wait for GPS fix on first startup

#define LED_BUILTIN GPIO_NUM_2 // Builtin LED pin
#define SD_CS GPIO_NUM_5 // SD card chip select pin

#define SONAR_RX GPIO_NUM_14 // Sonar sensor receive pin
#define SONAR_TX GPIO_NUM_32 // Sonar sensor transmit pin
#define SONAR_EN GPIO_NUM_33 //Sonar measurements are disabled when this pin is pulled low

#define GPS_RX GPIO_NUM_16
#define GPS_TX GPIO_NUM_17
#define GPS_CLOCK_EN GPIO_NUM_27
#define GPS_POLLING_HZ 100
#define GPS_DIFF_FROM_GMT 0

#define TEMP_SENSOR_ADDRESS 0x44
#define TEMP_EN GPIO_NUM_15

//-----------------------------------------------------------------------------------------------------||










//-----------------------------------------------------------------------------------------------------||
//---------- Declare Variables ------------------------------------------------------------------------||

// The RTC slow timer is driven by the RTC slow clock, typically 150kHz
uint64_t clock_start; //the rtc clock cycle count that we begin measuring at
uint32_t gps_millis_offset = millis();
const uint32_t rtc_slow_clk_hz = rtc_clk_slow_freq_get_hz();
const uint32_t rtc_abp_clk_hz = rtc_clk_apb_freq_get();
const uint64_t half_read_time = 1000000 * READ_TIME / 2;

// Data which should be preserved between sleep/wake cycles
RTC_DATA_ATTR uint32_t wakeCounter = 0;
RTC_DATA_ATTR uint64_t sleep_time = 0;
RTC_DATA_ATTR uint64_t sleep_cycles = 0;

// The format_buffer is overwritten by displayTime and unixTime, and used to format each line of data
char format_buf[FORMAT_BUF_SIZE];

// GPS ISR and Serial1 callback variables
volatile uint32_t num_gps_reads = 0;
volatile bool measurement_request = false;

// Objects to manage peripherals
Adafruit_GPS GPS(&Serial2);
Adafruit_SHT31 tempSensor = Adafruit_SHT31();
File data_file;

struct sensorData
{
    UnixTime time;
    float tempExt;
    float humExt;
    int dist;
    int repr(char *buf, uint32_t n);
    int fixType;
};

//------COMMUNICATION-------//
#define SENSOR_ID 1
// MAC address of receiver module
uint8_t broadcastAddress1[] = {0xC8, 0xC9, 0xA3, 0xC6, 0x1B, 0x44};
esp_now_peer_info_t peerInfo;

typedef struct sendStruct
{
  uint8_t sendMonth;
  uint8_t sendDay;
  uint8_t sendHour;
  uint8_t sendMinute;
  uint8_t sendSecond;
  uint16_t sendMillis;
  uint16_t sendDist;
  float sendTemp;
  float sendHum;
  int gaugeID;
} sendStruct;

sendStruct sendData;
//--------------------------//

//-----------------------------------------------------------------------------------------------------||










//-----------------------------------------------------------------------------------------------------||
//---------- Functions --------------------------------------------------------------------------------||

inline bool GPS_has_fix(Adafruit_GPS &gps) { return gps.fixquality >= 1; }

// returns time in microseconds since 'prev' as measured by rtc clk
inline uint64_t rtc_clk_usecs(uint64_t prev)
{
  return (1000000 * (rtc_time_get() - prev)) / rtc_slow_clk_hz;
}

// Return a current unix timestamp
UnixTime getTime(void)
{
    UnixTime stamp(GPS_DIFF_FROM_GMT);

    // If the GPS has a fix, use it to create a timestamp
    if(GPS_has_fix(GPS)) stamp.setDateTime(2000 + GPS.year, GPS.month, GPS.day, GPS.hour, GPS.minute, GPS.seconds);

    // Otherwise, use the esp32s builtin clock to estimate a time
    else stamp.getDateTime(sleep_time + rtc_clk_usecs(sleep_cycles) / 1000000);

    return stamp;
}

// Create a human readable time in PST
// ex: 10:53:32.023
char* displayTime(UnixTime now)
{
    snprintf(format_buf, FORMAT_BUF_SIZE, "%02d:%02d:%02d.%03d", GMT_to_PST(now.hour), now.minute, now.second, (millis() - gps_millis_offset)%1000);
    return format_buf;
}

// return signed latitude with the convention that north of the equator is positve
inline float latitude_signed(Adafruit_GPS &gps)
{
    return (gps.lat == 'N') ? gps.latitude : -1.0 * gps.latitude;
}

//return signed longitude with the convention that east of the prime meridian is positive
inline float longitude_signed(Adafruit_GPS &gps) {
    return (gps.lon == 'E') ? gps.longitude : -1.0 * gps.longitude;
}

// Write a message to a log file for debugging
void writeLog(const char* message)
{
  //Open log file and write to it
  File logFile = SD.open("/logFile.txt", FILE_WRITE);
  if(!logFile) return;
  //logFile.seek(logFile.size());
  logFile.printf("%d/%d/20%d, %s: %s\nwake count: %d\n", GPS.month, GPS.day, GPS.year, displayTime(getTime()), message, wakeCounter);
  if(GPS_has_fix(GPS)) logFile.printf("%f, %f, %f\n", latitude_signed(GPS), longitude_signed(GPS), GPS.altitude);
  logFile.close();
}

// Start GPS Clock
void startGPS(Adafruit_GPS &gps)
{
  // 9600 NMEA is the default baud rate for Adafruit MTK GPS's- some use 4800
  gps.begin(9600);
  writeLog("GPS Serial Enabled");

  // uncomment this line to turn on RMC (recommended minimum) and GGA (fix data) including altitude
  gps.sendCommand(PMTK_SET_NMEA_OUTPUT_RMCGGA);
  //gps.sendCommand(PMTK_SET_NMEA_OUTPUT_RMCONLY);
  writeLog("Minimum Recommended Enabled");

  // Set the update rate
  gps.sendCommand(PMTK_SET_NMEA_UPDATE_10HZ);
  writeLog("GPS Frequency Enabled");
}

// Create a unix timestamp of the current time including milliseconds
char* unixTime(UnixTime now)
{
    snprintf(format_buf, FORMAT_BUF_SIZE, "%d.%03d", now.getUnix(), (millis() - gps_millis_offset)%1000);
    return format_buf;
}

//Get a reading from the sonar
//negative readings imply an error
int32_t sonarMeasure(void)
{
    char inData[5] = {0}; //char array to read data into

    // Maxbotix reports "Rxxxx\L", where xxxx is a 4 digit mm distance, '\L' is carriage return
    // a measurement is 6 bytes
    if(Serial1.available() <= 5) return -1;
    //Measurements begin with 'R'
    while(Serial1.read() != 'R')
    {
        if(Serial1.available() <= 5) return -2;
    }

    Serial1.readBytes(inData, 4);
    if(Serial1.read() != 13) return -3; //check and discard carriage return

    // Convert character array to an integer number
    uint32_t result = atoi(inData);

    // Turn on LED if measuring properly
    // Maxbotix reports 500 if object is too close and 9999 is too far
    // Or you'll get 0 if wired improperly
    if ((result > 500) and (result < 9999))  digitalWrite(LED_BUILTIN, HIGH);
    else digitalWrite(LED_BUILTIN, LOW);

    return result;
  }

// represent this datum in ASCII text, and copy that representation into the given byte buffer
int sensorData::repr(char *buf, uint32_t n)
{
    return snprintf(buf, n, "%s, %d, %f, %f, %d\n",
    unixTime(time),
    dist,
    tempExt,
    humExt,
    fixType);
}

// Takes a measurement, formats it, and writes it to data storage in the given file
// Should be called whenever sonar sensor has data ready
sensorData readData(File &data_file)
{
    // Read data from the sonar, GPS clock, and temp/humidity sensor
    sensorData datum = {0};
    datum.dist = sonarMeasure();
    datum.time = getTime();
    datum.tempExt = celsius_to_fahrenheit(tempSensor.readTemperature());
    datum.humExt = tempSensor.readHumidity();
    datum.fixType = GPS_has_fix(GPS);

    // Send relevant data over espNow
    //------COMMUNICATION-------//
    #ifdef TRANSMIT
      sendData.sendMonth = datum.time.month;
      sendData.sendDay = datum.time.day;
      sendData.sendHour = datum.time.hour;
      sendData.sendMinute = datum.time.minute;
      sendData.sendSecond = datum.time.second;
      sendData.sendMillis = (millis() - gps_millis_offset)%1000;
      sendData.sendDist = datum.dist;
      sendData.sendTemp = datum.tempExt;
      sendData.sendHum  = datum.humExt;
      sendData.gaugeID = SENSOR_ID;
      esp_err_t sendResult = esp_now_send(0, (uint8_t *) &sendData, sizeof(sendData));
    #endif
    //--------------------------//

    // Write the data to a new line in the open SD card file
    int result = datum.repr(format_buf, FORMAT_BUF_SIZE);
    if(result < 0){}//format error
    else 
    {
        //some or all data succesfuly formatted into buffer
        data_file.write((uint8_t*)format_buf, (uint32_t)result);
        #ifdef DEBUG
          Serial.write((uint8_t*)format_buf, (uint32_t)result);
        #endif
    }

    return datum;
}

// Check or create header file
void sd_card_init(void)
{
    // Check if file exists and create one if not
    if (!SD.exists("/README.txt"))
    {
        File read_me = SD.open("/README.txt", FILE_WRITE);
        if(!read_me) return;

        // Create header with title, timestamp, and column names
        read_me.println("");
        read_me.printf(
            "Cal Poly Tide Sensor\n"
            "https://github.com/caleb-ad/wave-tide-sensor\n\n"
            "Data File format:\n"
            "Line   1: Latitude, Longitude, Altitude\n"
            "Line 'n': UNIX Time, Distance (mm), External Temp (F), Humidity (%)\n",
            unixTime(getTime()));
        read_me.close();

        SD.mkdir("/Data");
    }
}

// Creates a file in the Data folder. This file will be closed before the esp32 enters sleep
// If the GPS has a fix or has had a fix the file is named using the current time in hex.
// Otherwise the file is named wakecount_millisecondsawake
File create_file()
{
    //filenames are at most 8 characters + 6("/Data/") + 4(".txt") + null terminator = 19
    if(GPS_has_fix(GPS) || sleep_time != 0) snprintf(format_buf, 19 , "/Data/%x.txt", getTime().getUnix());
    else snprintf(format_buf, 19, "/Data/%x_%x.txt", wakeCounter, millis());

    File file = SD.open(format_buf, FILE_WRITE, true);
    assert(file);
    return file;
}

// Powers down all peripherals, Sleeps until woken after set interval, runs setup() again
void goto_sleep(void)
{
    writeLog("sleeping");

    // Turn everything off
    digitalWrite(LED_BUILTIN, LOW);
    digitalWrite(GPS_CLOCK_EN, LOW);
    digitalWrite(TEMP_EN, LOW);
    digitalWrite(SONAR_EN, LOW);
    gpio_hold_en(GPS_CLOCK_EN); // Make sure clock is off
    gpio_hold_en(TEMP_EN); // Make sure temp sensor is off
    gpio_hold_en(SONAR_EN); // Make sure Maxbotix is off
    gpio_deep_sleep_hold_en();

    sleep_time = getTime().getUnix();
    sleep_cycles = rtc_time_get();

    uint64_t sleepTime = 0;
    
    // Schedule to wake up so that the next measurements are centered at the next scheduled measurement time
    if(GPS_has_fix(GPS))
    {
        uint64_t next_measurement = (MINUTE_ALLIGN - (GPS.minute % MINUTE_ALLIGN)) * (60 * 1000000) - (GPS.seconds * 1000000) - ((millis() - gps_millis_offset) % 1000) * 1000;
        sleepTime = next_measurement > half_read_time ?
            next_measurement - half_read_time :
            next_measurement + (MINUTE_ALLIGN * 60 * 1000000) - half_read_time;
        esp_sleep_enable_timer_wakeup(sleepTime);
            
    }
    else
    {
        // When the GPS does not have a fix sleep for the correct interval, the GPS may have previously had a fix
        //*This could overflow
        sleepTime = 1000000 * 60 * MINUTE_ALLIGN  - rtc_clk_usecs(clock_start);
        esp_sleep_enable_timer_wakeup(sleepTime);
    }

    // Print sleep time info
    #ifdef DEBUG
    Serial.printf("Going to sleep at %s\n", displayTime(getTime()));
    Serial.printf("Sleeping for %d seconds\n", sleepTime/1000000);
    if(GPS_has_fix(GPS)) Serial.println("GPS has fix");
    #endif

    esp_deep_sleep_start();
}

// Callback function for Serial1.onReceive
void sonarDataReady(void)
{
    // When new data is available on the Serial1 line, update the measurement request
    measurement_request = true;
}

// Callback function for espNow
void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status)
{
  // Do nothing special
}

// every 10ms schedule a read from the GPS, when
bool gps_polling_isr(void* arg)
{
    num_gps_reads += 1;
    timer_group_clr_intr_status_in_isr(timer_group_t::TIMER_GROUP_0, timer_idx_t::TIMER_0);
    return false;
}

void getFix(Adafruit_GPS &gps)
{
  #ifdef DEBUG
    Serial.printf("Waiting %d seconds for GPS fix\n", FIX_DELAY);
  #endif
  
  uint32_t start = millis();
  uint8_t fixType = 0;

  // Wait a minute to try and get a fix
  while ((millis() - start) < FIX_DELAY*1000)
  {
    gps.read();
    if(gps.newNMEAreceived())
    {
      gps.parse(gps.lastNMEA());  // this also sets the newNMEAreceived() flag to false
      fixType = gps.fixquality;
    }
    
    if (fixType)
    {
      #ifdef DEBUG
        Serial.printf("GPS fix found after %d seconds\n", (millis()-start)/1000);
      #endif
      return;
    }
  }

  #ifdef DEBUG
    Serial.println("No GPS fix acquired");
  #endif
}

//-----------------------------------------------------------------------------------------------------||










//-----------------------------------------------------------------------------------------------------||
//---------- Main Program -----------------------------------------------------------------------------||

void setup(void)
{
  // Clock cycle count when we begin measuring
  clock_start = rtc_time_get();

  // Assert that constants and defines are in valid state
  assert(READ_TIME < MINUTE_ALLIGN * 60);

  // Setup for SD card
  pinMode(SD_CS, OUTPUT);
  assert(SD.begin(SD_CS));

  // Enable serial port peripherals
  Serial1.begin(9600, SERIAL_8N1, SONAR_RX, SONAR_TX); // Maxbotix requires 9600bps
  Serial1.onReceive(sonarDataReady, true); // register callback
  Serial1.setRxTimeout(10);
  startGPS(GPS); //uses Serial2
  writeLog("Serial Ports Enabled");

  // configure timer to manage GPS polling
  timer_config_t timer_polling_config;
  timer_polling_config.alarm_en = timer_alarm_t::TIMER_ALARM_EN;
  timer_polling_config.auto_reload = timer_autoreload_t::TIMER_AUTORELOAD_EN;
  timer_polling_config.counter_dir = timer_count_dir_t::TIMER_COUNT_UP;
  timer_polling_config.divider = 2; //should be in [2, 65536]
  timer_init(timer_group_t::TIMER_GROUP_0, timer_idx_t::TIMER_0, &timer_polling_config);
  timer_set_alarm_value(timer_group_t::TIMER_GROUP_0, timer_idx_t::TIMER_0, rtc_abp_clk_hz / (2 * GPS_POLLING_HZ)); // configure timer to count 10 millis
  timer_isr_callback_add(timer_group_t::TIMER_GROUP_0, timer_idx_t::TIMER_0, gps_polling_isr, nullptr, ESP_INTR_FLAG_LOWMED);
  timer_enable_intr(timer_group_t::TIMER_GROUP_0, timer_idx_t::TIMER_0);
  timer_start(timer_group_t::TIMER_GROUP_0, timer_idx_t::TIMER_0);

  //Turn on relevant pins
  gpio_hold_dis(GPIO_NUM_15);
  gpio_hold_dis(GPIO_NUM_33);
  gpio_hold_dis(GPIO_NUM_27);
  pinMode(GPS_CLOCK_EN, OUTPUT);
  pinMode(TEMP_EN, OUTPUT);
  pinMode(SONAR_EN, OUTPUT);
  digitalWrite(GPS_CLOCK_EN, HIGH); //Hold clock high
  digitalWrite(TEMP_EN, HIGH); //Hold high to supply power to temp sensor
  digitalWrite(SONAR_EN, HIGH); //Hold high to begin measuring with sonar
  writeLog("Start/Stop Enabled");

  //Setup for temp/humidity
  tempSensor.begin(TEMP_SENSOR_ADDRESS); //Hex Address for new I2C pins
  writeLog("Temp Sensor Enabled");

  //Setup for LED
  pinMode(LED_BUILTIN, OUTPUT);
  writeLog("LEDs enabled");

  #ifdef DEBUG
    Serial.begin(115200); //Serial monitor
    Serial.printf("Wake Counter: %d\n", wakeCounter);
  #endif

  // Run Setup, check SD file every 1000th wake cycle
  if ((wakeCounter % 1000) == 0)
  {
      wakeCounter = 0;
      sd_card_init();
      getFix(GPS);
      //TODO get measurements to allign with 15 min intervals
  }
  wakeCounter += 1;
    
  // Setup for espNow communication
  //------COMMUNICATION-------//
  #ifdef TRANSMIT
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
    memcpy(peerInfo.peer_addr, broadcastAddress1, 6);

    if (esp_now_add_peer(&peerInfo) != ESP_OK)
    {
      Serial.println("Failed to add peer");
      return;
    }
  #endif
  //--------------------------//

  data_file = create_file();

}

void loop(void)
{
    // Get new information from GPS
    while(num_gps_reads > 0)
    {
        GPS.read(); //if GPS.read() takes longer than the GPS polling frequency, execution may get stuck in this loop
        num_gps_reads -= 1;
        if(GPS.newNMEAreceived()) {
            // a tricky thing here is if we print the NMEA sentence, or data
            // we end up not listening and catching other sentences!
            // so be very wary if using OUTPUT_ALLDATA and trying to print out data
            //Serial.println(GPS.lastNMEA());   // this also sets the newNMEAreceived() flag to false
            GPS.parse(GPS.lastNMEA());  // this also sets the newNMEAreceived() flag to false
            gps_millis_offset = millis() - GPS.milliseconds;
            Serial.println(gps_millis_offset);
        }
    }

    // If there is new sonar data, read it
    if(measurement_request)
    {
        measurement_request = false;
        readData(data_file);
    }

    // If it is time to go to sleep, close the data file and enter sleep mode
    if(rtc_clk_usecs(clock_start) >= READ_TIME * 1000000)
    {
        writeLog("Finished measurement");
        #ifdef CONTINUOUS
        data_file.close();
        sleep_time = getTime().getUnix();
        clock_start = rtc_time_get();
        data_file = create_file();
        #else
        //close data_file makes sure all data is written to SD card before cleaning up resources
        data_file.close();
        goto_sleep();
        #endif
    }
}
//-----------------------------------------------------------------------------------------------------||