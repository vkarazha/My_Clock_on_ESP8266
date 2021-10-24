//!!! #include <Wire.h>
//!!! #include <RtcDS3231.h>                  // Include RTC library by Makuna: https://github.com/Makuna/Rtc
//#include <OneWire.h>
//#include <FS.h>                             // Instructions on http://arduino.esp8266.com/Arduino/versions/2.3.0/doc/filesystem.html#uploading-files-to-file-system
//#include <ESP8266WiFi.h>
//#include <ESP8266WebServer.h>
#include <NTPClient.h>
#include <ESP8266HTTPUpdateServer.h>
#include <DallasTemperature.h>
#include <FastLED.h>

#define countof(a) (sizeof(a) / sizeof(a[0]))
#define NUM_LEDS 58                           // Total of 58 LEDs     
#define DATA_PIN D5                           // Data output on D5
#define ONE_WIRE_BUS D6                       // Temp sensor on D6  
#define MILLI_AMPS 2400 
#define WIFIMODE 1                            //!!!   0 = Only Soft Access Point, 1 = Only connect to local WiFi network with UN/PW, 2 = Both
#define DOT1 28 
#define DOT2 29 

#if defined(WIFIMODE) && (WIFIMODE == 0 || WIFIMODE == 2)
  const char* APssid = "CLOCK_AP";        
  const char* APpassword = "1234567890"; 
#endif
  
#if defined(WIFIMODE) && (WIFIMODE == 1 || WIFIMODE == 2)
  #include "Settings.h"                       // Create this file in the same directory as the .ino file and add your wifi settings (#define SID "your_ssid" and on the second line #define PSW "your_password")
  const char *ssid = SID;
  const char *password = PSW;
#endif

//!!! RtcDS3231<TwoWire> Rtc(Wire);
ESP8266WebServer server(80);
ESP8266HTTPUpdateServer httpUpdateServer;
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 10800, 3600123);   // GMT+3 :   3 * 3600 = 10800

// Settings
uint32_t prevTime = 0;
uint32_t currentMillis;
uint32_t countdownMillis;
uint32_t endCountDownMillis;
//uint8_t r_val = 255;
//uint8_t g_val = 0;
//uint8_t b_val = 0;
uint8_t brightness = 120;
uint8_t colorNum = 1;
uint8_t tempSymbol = 12;                      // 12=Celcius, 13=Fahrenheit check 'numbers'
uint8_t clockMode = 0;                        // Clock modes: 0=Clock, 1=Countdown, 2=Temperature, 3=Scoreboard
uint8_t hourFormat = 24;                      // 12 or 24 hour format
uint8_t scoreboardLeft = 0;
uint8_t scoreboardRight = 0;
uint8_t lastDigit = 0;
bool dotsOn = true;
bool autoChange = true;
float temperatureCorrection = -3.0;
CRGB LEDs[NUM_LEDS];
CRGB color = CRGB::Aqua;                      // Default color
CRGB countdownColor = CRGB::Green;
CRGB scoreboardColorLeft = CRGB::Green;
CRGB scoreboardColorRight = CRGB::Red;
CRGB colorTable[] = {                         // Colors for autoChange
  CRGB::Amethyst,
  CRGB::Aqua,
  CRGB::Yellow,
  CRGB::Blue,
  CRGB::Chartreuse,
  CRGB::LightCoral,
  CRGB::Green,
  CRGB::Tomato,
  CRGB::DarkOrange,
  CRGB::DeepPink,
  CRGB::Gold,
  CRGB::Magenta,
  CRGB::GreenYellow,
  CRGB::Salmon,
  CRGB::Red,
  CRGB::Orchid
};
uint16_t numbers[] = {
  0b11111111111100,  // [0] 0
  0b11000000001100,  // [1] 1
  0b00111100111111,  // [2] 2
  0b11110000111111,  // [3] 3
  0b11000011001111,  // [4] 4
  0b11110011110011,  // [5] 5
  0b11111111110011,  // [6] 6
  0b11000000111100,  // [7] 7
  0b11111111111111,  // [8] 8
  0b11110011111111,  // [9] 9
  0b00000000000000,  // [10] off
  0b00000011111111,  // [11] degrees symbol
  0b00111111110000,  // [12] C(elsius)
  0b00001111110011   // [13] F(ahrenheit)
};
uint8_t segmentStart[] = { 44, 30, 14, 0 };   // Start of each segment. Segment from left to right: 0, 1, 2, 3  

void setup() {
  Serial.begin(115200); 
  delay(200);

/*!!! // RTC DS3231 Setup
  Rtc.Begin();    
  RtcDateTime compiled = RtcDateTime(__DATE__, __TIME__);

  if (!Rtc.IsDateTimeValid()) {
      if (Rtc.LastError() != 0) {  
          // We have a communications error see https://www.arduino.cc/en/Reference/WireEndTransmission for what the number means
          Serial.print(F("RTC communications error = "));
          Serial.println(Rtc.LastError());
      } else {
          // Common Causes:
          //    1) first time you ran and the device wasn't running yet
          //    2) the battery on the device is low or even missing
          Serial.println(F("RTC lost confidence in the DateTime!"));
          // following line sets the RTC to the date & time this sketch was compiled
          // it will also reset the valid flag internally unless the Rtc device is
          // having an issue
          Rtc.SetDateTime(compiled);
      }
  }!!!*/

  WiFi.setSleepMode(WIFI_NONE_SLEEP);  
  delay(200);
  FastLED.addLeds<WS2812B, DATA_PIN, GRB>(LEDs, NUM_LEDS);  
  FastLED.setDither(false);
  FastLED.setCorrection(TypicalLEDStrip);
  FastLED.setMaxPowerInVoltsAndMilliamps(5, MILLI_AMPS);
  fill_solid(LEDs, NUM_LEDS, CRGB::Black);
  FastLED.setBrightness(brightness);
  FastLED.show();

  // WiFi - AP Mode or both
#if defined(WIFIMODE) && (WIFIMODE == 0 || WIFIMODE == 2) 
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP(APssid, APpassword);            // IP is usually 192.168.4.1
  Serial.println();
  Serial.print(F("SoftAP IP: "));
  Serial.println(WiFi.softAPIP());
#endif

  // WiFi - Local network Mode or both
#if defined(WIFIMODE) && (WIFIMODE == 1 || WIFIMODE == 2) 
  uint8_t count = 0;
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    // Stop if cannot connect
    if (count >= 60) {
      Serial.println(F("Could not connect to local WiFi."));      
      return;
    }
    delay(500);
    Serial.print(F("."));
    timeClient.begin();
    count++;
  }
  Serial.print(F("Local IP: "));
  Serial.println(WiFi.localIP());
  IPAddress ip = WiFi.localIP();
  Serial.println(ip[3]);
#endif   

  httpUpdateServer.setup(&server);

  // Handlers
  server.on("/color", HTTP_POST, []() {    
    uint8_t r_val = server.arg("r").toInt();
    uint8_t g_val = server.arg("g").toInt();
    uint8_t b_val = server.arg("b").toInt();
    color = CRGB(r_val, g_val, b_val);
    autoChange = false;
    server.send(200, "text/json", "{\"result\":\"ok\"}");
  });

  server.on("/setdate", HTTP_POST, []() { 
    // Sample input: date = "Dec 06 2009", time = "12:34:56"
    // Jan Feb Mar Apr May Jun Jul Aug Sep Oct Nov Dec
/*!!!    String datearg = server.arg("date");
    String timearg = server.arg("time");
    char d[12];
    char t[9];
    datearg.toCharArray(d, 12);
    timearg.toCharArray(t, 9);
    RtcDateTime compiled = RtcDateTime(d, t);
    Rtc.SetDateTime(compiled);   
!!!*/
    clockMode = 0;     
    server.send(200, "text/json", "{\"result\":\"ok\"}");
  });

  server.on("/brightness", HTTP_POST, []() {    
    brightness = server.arg("brightness").toInt();    
    FastLED.setBrightness(brightness);
    server.send(200, "text/json", "{\"result\":\"ok\"}");
  });
  
  server.on("/countdown", HTTP_POST, []() {    
    countdownMillis = server.arg("ms").toInt();     
    uint8_t cd_r_val = server.arg("r").toInt();
    uint8_t cd_g_val = server.arg("g").toInt();
    uint8_t cd_b_val = server.arg("b").toInt();
    countdownColor = CRGB(cd_r_val, cd_g_val, cd_b_val); 
    endCountDownMillis = millis() + countdownMillis;
    allBlank(); 
    clockMode = 1;     
    server.send(200, "text/json", "{\"result\":\"ok\"}");
  });

  server.on("/temperature", HTTP_POST, []() {   
    temperatureCorrection = server.arg("correction").toInt();
    tempSymbol = server.arg("symbol").toInt();
    clockMode = 2;     
    server.send(200, "text/json", "{\"result\":\"ok\"}");
  });  

  server.on("/scoreboard", HTTP_POST, []() {   
    scoreboardLeft = server.arg("left").toInt();
    scoreboardRight = server.arg("right").toInt();
    scoreboardColorLeft = CRGB(server.arg("rl").toInt(),server.arg("gl").toInt(),server.arg("bl").toInt());
    scoreboardColorRight = CRGB(server.arg("rr").toInt(),server.arg("gr").toInt(),server.arg("br").toInt());
    clockMode = 3;     
    server.send(200, "text/json", "{\"result\":\"ok\"}");
  });  

  server.on("/hourformat", HTTP_POST, []() {   
    hourFormat = server.arg("hourformat").toInt();
    clockMode = 0;     
    server.send(200, "text/json", "{\"result\":\"ok\"}");
  }); 

  server.on("/clock", HTTP_POST, []() {       
    clockMode = 0;     
    server.send(200, "text/json", "{\"result\":\"ok\"}");
  });  

  server.on("/auto", HTTP_POST, []() { 
    lastDigit = 55;      
    autoChange = true;
    server.send(200, "text/json", "{\"result\":\"ok\"}");
  });  

  server.serveStatic("/", SPIFFS, "/", "max-age=86400");
  server.begin();     
  SPIFFS.begin();
  sensors.begin();
  colorNum = random(16);
}

void loop(){
  server.handleClient(); 
  
  currentMillis = millis();  
  if (currentMillis - prevTime >= 1000) {
    prevTime = currentMillis;

    if (clockMode == 0) {
      updateClock();
    } else if (clockMode == 1) {
      updateCountdown();
    } else if (clockMode == 2) {
      updateTemperature();      
    } else if (clockMode == 3) {
      updateScoreboard();          
    }

    FastLED.show();
  }   
}

void displayNumber(uint8_t number, uint8_t segment, CRGB color) {
  for (uint8_t i=0; i<14; i++){
    yield();
    LEDs[i + segmentStart[segment]] = ((numbers[number] & 1 << i) == 1 << i) ? color : CRGB::Black;
  } 
}

void allBlank() {
  for (uint8_t i=0; i<NUM_LEDS; i++) {
    LEDs[i] = CRGB::Black;
  }
  FastLED.show();
}

void updateClock() {  
  while(!timeClient.update()){timeClient.forceUpdate();}
  
  int hour = timeClient.getHours();
  int mins = timeClient.getMinutes();
  //int secs = timeClient.getSeconds();

  if (hourFormat == 12 && hour > 12)
    hour = hour - 12;

  uint8_t h1 = hour / 10;
  uint8_t h2 = hour % 10;
  uint8_t m1 = mins / 10;
  uint8_t m2 = mins % 10;  
  //uint8_t s1 = secs / 10;
  //uint8_t s2 = secs % 10;

  if (autoChange && (m2 != lastDigit)) {      // Change color every minute
    color = colorTable[colorNum++];
    if (colorNum == sizeof(colorTable)) 
      colorNum = 0;
    lastDigit = m2;
  }
  
  if (h1 == 0)
    h1 = 10;                                  // Blank

  displayNumber(h1,3,color);
  displayNumber(h2,2,color);
  displayNumber(m1,1,color);
  displayNumber(m2,0,color); 
  displayDots(color);  
}

void updateCountdown() {
  if (countdownMillis == 0 && endCountDownMillis == 0) 
    return;
    
  uint32_t restMillis = endCountDownMillis - millis();
  uint32_t hours   = ((restMillis / 1000) / 60) / 60;
  uint32_t minutes = (restMillis / 1000) / 60;
  uint32_t seconds = restMillis / 1000;
  uint8_t remSeconds = seconds - (minutes * 60);
  uint8_t remMinutes = minutes - (hours * 60); 
  uint8_t h1 = hours / 10;
  uint8_t h2 = hours % 10;
  uint8_t m1 = remMinutes / 10;
  uint8_t m2 = remMinutes % 10;  
  uint8_t s1 = remSeconds / 10;
  uint8_t s2 = remSeconds % 10;

  CRGB color = countdownColor;
  if (restMillis <= 60000) {
    color = CRGB::Red;
  }

  if (hours > 0) {                       // hh:mm
    displayNumber(h1,3,color); 
    displayNumber(h2,2,color);
    displayNumber(m1,1,color);
    displayNumber(m2,0,color);  
  } else {                               // mm:ss   
    displayNumber(m1,3,color);
    displayNumber(m2,2,color);
    displayNumber(s1,1,color);
    displayNumber(s2,0,color);  
  }

  displayDots(color);  

  if (hours <= 0 && remMinutes <= 0 && remSeconds <= 0) {
    countdownMillis = 0;
    endCountDownMillis = 0;
    return;
  }  
}

void endCountdown() {
  allBlank();
  for (uint8_t i=0; i<NUM_LEDS; i++) {
    if (i>0)
      LEDs[i-1] = CRGB::Black;
    
    LEDs[i] = CRGB::Red;
    FastLED.show();
    delay(25);
  }  
}

void displayDots(CRGB color) {
  if (dotsOn) 
    LEDs[DOT1] = LEDs[DOT2] = color;
  else
    LEDs[DOT1] = LEDs[DOT2] = CRGB::Black;
  dotsOn = !dotsOn;  
}
 
void hideDots() {
  LEDs[DOT1] = LEDs[DOT2] = CRGB::Black;
}

void updateTemperature() {
/*!!!  RtcTemperature temp = Rtc.GetTemperature();
  float ftemp = temp.AsFloatDegC();             // Sensor temp
  float ctemp = ftemp + temperatureCorrection;  // Corrected temp
!!!*/
  sensors.requestTemperatures();                // Get temp from DS18B20
  float tempC = sensors.getTempCByIndex(0); 

  if (tempSymbol == 13)
    tempC = tempC * 1.8 + 32;

  uint8_t t1 = uint8_t(tempC) / 10;
  uint8_t t2 = uint8_t(tempC) % 10;

  //CRGB color = CRGB(r_val, g_val, b_val);
  displayNumber(t1,3,color);
  displayNumber(t2,2,color);
  displayNumber(11,1,color);
  displayNumber(tempSymbol,0,color);
  hideDots();
}

void updateScoreboard() {
  uint8_t sl1 = scoreboardLeft / 10;
  uint8_t sl2 = scoreboardLeft % 10;
  uint8_t sr1 = scoreboardRight / 10;
  uint8_t sr2 = scoreboardRight % 10;

  displayNumber(sl1,3,scoreboardColorLeft);
  displayNumber(sl2,2,scoreboardColorLeft);
  displayNumber(sr1,1,scoreboardColorRight);
  displayNumber(sr2,0,scoreboardColorRight);
  hideDots();
}
