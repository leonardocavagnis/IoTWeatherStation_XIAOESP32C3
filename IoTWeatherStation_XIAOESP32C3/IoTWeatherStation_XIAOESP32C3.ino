/**
  ******************************************************************************
  * @file    IoTWeatherStation_XIAOESP32C3.ino
  * @author  Leonardo Cavagnis
  * @version V1.0.0
  *
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include <WiFi.h>               //Include the standard Wi-Fi library
#include "DHT.h"                //Include the Grove Humidity&Temperature sensor library
#include <U8x8lib.h>            //Include the U8g2 library
#include <PCF8563.h>            //Include the RTC library
#include "time.h"               //Include time library to manipulate date and time
#include <ArduinoMqttClient.h>  //Include the MQTT library

/* Private define ------------------------------------------------------------*/
#define WIRE_SDA_PIN    4
#define WIRE_SCL_PIN    5

#define BUTTON_PIN      3

#define BUZZER_PIN      A3
#define BUZZER_FREQ     370 //[us] 370us = 1/(Resonant freq) = 1/(2700Hz)
#define BUZZER_DURATION 500 //[ms]

/* Private constants -------------------------------------------------------------*/
//Set the SSID and password of the network you want to connect
const char* ssid                = "<INSERT_YOUR_SSID>";
const char* password            = "<INSERT_YOUR_PASSWORD>";   
//Define the NTP server to use, the GMT offset, and the daylight offset
const char* ntpServer           = "pool.ntp.org";
const long  gmtOffset_sec       = 3600; //GMT+1 (Rome)
const int   daylightOffset_sec  = 3600;
//MQTT broker address
const char  mqtt_broker[]       = "mqtt3.thingspeak.com";
int         mqtt_port           = 1883;
//MQTT credentials
const char  mqtt_id[]           = "<INSERT_YOUR_ID>";
const char  mqtt_username[]     = "<INSERT_YOUR_USERNAME>";
const char  mqtt_pwd[]          = "<INSERT_YOUR_PASSWORD>";
//MQTT topics
const char  topic_temp[]        = "channels/<INSERT_YOUR_CHANNEL_ID>/publish/fields/field1";
const char  topic_hum[]         = "channels/<INSERT_YOUR_CHANNEL_ID>/publish/fields/field2";

/* Private variables ---------------------------------------------------------*/
DHT           dht(DHT20);  //Select DHT20 sensor type
U8X8_SSD1306_128X64_NONAME_HW_I2C oled(WIRE_SCL_PIN, WIRE_SDA_PIN, U8X8_PIN_NONE); //Initialize the SSD1306 controller of the display
PCF8563       rtc;
WiFiClient    wifiClient;
MqttClient    mqttClient(wifiClient);

unsigned long currentMillis;
unsigned long previousMillis;
int           buttonState;
int           buttonStatePrev;
bool          page;

/* Functions -----------------------------------------------------------------*/
void setup() {
  Serial.begin(115200);

  pinMode(BUZZER_PIN, OUTPUT); //Initialize the pin connected to the buzzer as output
  
  Wire.begin(); //Initialize the Wire protocol. DHT20 uses I2C to communicate
  dht.begin();  //Initialize DHT sensor

  oled.begin(); //Initialize the communication with the display
  oled.setFlipMode(0); //Set the display orientation (0=0°, 1=180°)
  oled.setFont(u8x8_font_chroma48medium8_r); //Set the font of the text to be displayed
  oled.clear();
  
  Serial.print("Connecting to ");
  Serial.print(ssid);
  
  //Connect to the network
  WiFi.begin(ssid, password);

  int wifiConnAttempts = 0;
  //Checks the status of the connection
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
   
    wifiConnAttempts++;
    if (wifiConnAttempts > 10) {
      //Display error message
      oled.setCursor(2, 2);
      oled.print("WiFi Error");
      
      //If the WiFi connection procedure fails after 10 attempts, emit an acoustic signal
      while (1) {
        playBuzzerTone(BUZZER_FREQ, BUZZER_DURATION);   
        delay(200); 
      }
    }
  }
  Serial.println("");
  
  //Once the connection is established, print the assigned IP address
  Serial.println("WiFi connected - IP address:");
  Serial.println(WiFi.localIP());

  //Get the current time and date from the NTP Server
  struct tm timeinfo;
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  getLocalTime(&timeinfo);

  rtc.init(); //Initialize the PCF8563 RTC component
  rtc.stopClock();

  //Sets the date and time retrieved from the NTP server
  rtc.setYear((timeinfo.tm_year+1900)-2000); // tm_year=(YYYY-1900), setYear(YYYY-2000)
  rtc.setMonth(timeinfo.tm_mon+1); //tm_mon = [0(jan)-11(dec)]
  rtc.setDay(timeinfo.tm_mday);
  rtc.setHour(timeinfo.tm_hour);
  rtc.setMinut(timeinfo.tm_min);
  rtc.setSecond(timeinfo.tm_sec);

  rtc.startClock(); //Start the PCF8563 RTC component

  //Set MQTT credentials
  mqttClient.setId(mqtt_id);
  mqttClient.setUsernamePassword(mqtt_username, mqtt_pwd);
  
  //Connect to the MQTT broker
  if (!mqttClient.connect(mqtt_broker, mqtt_port)) {
    Serial.print("MQTT connection failed! Error code = ");
    Serial.println(mqttClient.connectError());
    
    //Display error message
    oled.setCursor(2, 2);
    oled.print("MQTT Error");

    //If MQTT connection procedure fails, emit an acoustic signal
    while (1) {
      playBuzzerTone(BUZZER_FREQ, BUZZER_DURATION);   
      delay(200); 
    }
  }
  Serial.println("You're connected to the MQTT broker!");

  //Initialize button pin as digital input
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  buttonStatePrev = digitalRead(BUTTON_PIN);
  page = true;

  previousMillis = millis();
}

void loop() {
 //Send MQTT keep alive to ensure the broker that the client is still connected
 mqttClient.poll();
 
 //Read the current state of the button
 buttonState = digitalRead(BUTTON_PIN);
  
 //Check if the button is pressed
 if (buttonState == HIGH && buttonStatePrev == LOW) {
  Serial.println("Button press");
  page = !page;
  oled.clear();
 }
 buttonStatePrev = buttonState;
 
 currentMillis = millis();

 if (currentMillis - previousMillis >= 1000) {
  float temp_hum_val[2] = {0};
 
  //Read the temperature and humidity values from the sensor
  if (!dht.readTempAndHumidity(temp_hum_val)) {
   Serial.print("Humidity: ");
   Serial.print(temp_hum_val[0]);
   Serial.print("%\t");
   Serial.print("Temperature: ");
   Serial.print(temp_hum_val[1]);
   Serial.println("°C");
  } else {
   Serial.println("Failed to get temprature and humidity value.");
  }
  
  //Display the selected page
  if (page) {
    //Page 1: Temperature&Humidity
    //Print the humidity value at position [2,2] of the display
    oled.setCursor(2, 2);
    oled.print("Humi(%):");
    oled.print(temp_hum_val[0]);
    //Print the temperature value at position [2,3] of the display
    oled.setCursor(2, 3);
    oled.print("Temp(C):");
    oled.print(temp_hum_val[1]);
  } else {
    //Page 2: Date&Time, WiFi network name, MQTT server name
    //Retrieve the current time from the RTC
    Time nowTime = rtc.getTime();
    oled.setCursor(2, 2);
    oled.print(nowTime.day);
    oled.print("/");
    oled.print(nowTime.month);
    oled.print("/");
    oled.print("20");
    oled.print(nowTime.year);
    oled.setCursor(2, 3);
    oled.print(nowTime.hour);
    oled.print(":");
    oled.print(nowTime.minute);
    oled.print(":");
    oled.println(nowTime.second);
    oled.setCursor(2, 4);
    if(WiFi.status() == WL_CONNECTED) oled.println("Wi-Fi: OK");
    else oled.println("Wi-Fi: No");
    oled.setCursor(2, 5);
    if(mqttClient.connected()) oled.println("MQTT: OK");
    else oled.println("MQTT: No");
  }

  static int mqttSendCounter = 0;
  mqttSendCounter++;

  if (mqttSendCounter == 15) {
    //Send temperature data to the respective topic
    mqttClient.beginMessage(topic_temp);
    mqttClient.print(temp_hum_val[1]);
    mqttClient.endMessage();
  } else if (mqttSendCounter == 30) {
    //Send humidity data to the respective topic
    mqttClient.beginMessage(topic_hum);
    mqttClient.print(temp_hum_val[0]);
    mqttClient.endMessage();

    mqttSendCounter = 0;
  }
  
  previousMillis = currentMillis;
 }
}

/*
 * Emit an acoustic signal using a passive buzzer attached to a digital pin
 * Input: 
 *  frequency: frequency period in microseconds (us) of the acoustic signal
 *  duration: duration in milliseconds (ms) of the acoustic signal
 * Output: None
 */
void playBuzzerTone(int frequency, int duration) {
  for (long i = 0; i < duration * 1000L; i += frequency) {
    digitalWrite(BUZZER_PIN, HIGH);
    delayMicroseconds(frequency/2);
    digitalWrite(BUZZER_PIN, LOW);
    delayMicroseconds(frequency/2);
  }
}

/**** END OF FILE ****/
