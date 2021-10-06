//!!! #include <Wire.h>
//!!! #include <RtcDS3231.h>                  // Include RTC library by Makuna: https://github.com/Makuna/Rtc
#include <NTPClient.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPUpdateServer.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <FastLED.h>
#include <FS.h>                               // Instructions on http://arduino.esp8266.com/Arduino/versions/2.3.0/doc/filesystem.html#uploading-files-to-file-system

#define countof(a) (sizeof(a) / sizeof(a[0]))
#define NUM_LEDS 58                           // Total of 58 LEDs     
#define DATA_PIN D5                           // Data output on D5
#define ONE_WIRE_BUS D6                       // Temp sensor on D6  
#define MILLI_AMPS 2400 
#define WIFIMODE 1                            //!!!   0 = Only Soft Access Point, 1 = Only connect to local WiFi network with UN/PW, 2 = Both

#if defined(WIFIMODE) && (WIFIMODE == 0 || WIFIMODE == 2)
  const char* APssid = "CLOCK_AP";        
  const char* APpassword = "1234567890"; 
#endif
  
#if defined(WIFIMODE) && (WIFIMODE == 1 || WIFIMODE == 2)
  #include "Settings.h"                       // Create this file in the same directory as the .ino file and add your wifi settings (#define SID YOURSSID and on the second line #define PSW YOURPASSWORD)
  const char *ssid = SID;
  const char *password = PSW;
#endif

//!!! RtcDS3231<TwoWire> Rtc(Wire);
ESP8266WebServer server(80);
ESP8266HTTPUpdateServer httpUpdateServer;
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 10800, 3600123);// 10800 - time shift in seconds from UTC // https://www.ntppool.org/zone/@ - other servers
CRGB LEDs[NUM_LEDS];

// Settings
unsigned long prevTime = 0;
byte r_val = 255;
byte g_val = 0;
byte b_val = 0;
bool dotsOn = true;
bool autoChange = true;
byte brightness = 130;
byte colorNum = 0;
CRGB color = CRGB::DarkOrchid;                // Default color
float temperatureCorrection = -3.0;
byte temperatureSymbol = 12;                  // 12=Celcius, 13=Fahrenheit check 'numbers'
byte clockMode = 0;                           // Clock modes: 0=Clock, 1=Countdown, 2=Temperature, 3=Scoreboard
unsigned long countdownMilliSeconds;
unsigned long endCountDownMillis;
byte hourFormat = 24;                         // Change this to 12 if you want default 12 hours format instead of 24               
CRGB countdownColor = CRGB::Green;
byte scoreboardLeft = 0;
byte scoreboardRight = 0;
CRGB scoreboardColorLeft = CRGB::Green;
CRGB scoreboardColorRight = CRGB::Red;
byte last_digit = 0;
long ColorTable[16] = {                       // Random colors
  CRGB::Amethyst,
  CRGB::Aqua,
  CRGB::Blue,
  CRGB::Chartreuse,
  CRGB::DarkGreen,
  CRGB::DarkMagenta,
  CRGB::DarkOrange,
  CRGB::DeepPink,
  CRGB::Fuchsia,
  CRGB::Gold,
  CRGB::GreenYellow,
  CRGB::LightCoral,
  CRGB::Tomato,
  CRGB::Salmon,
  CRGB::Red,
  CRGB::Orchid
};
long numbers[] = {
  0b00111111111111,  // [0] 0
  0b00110000000011,  // [1] 1
  0b11111100111100,  // [2] 2
  0b11111100001111,  // [3] 3
  0b11110011000011,  // [4] 4
  0b11001111001111,  // [5] 5
  0b11001111111111,  // [6] 6
  0b00111100000011,  // [7] 7
  0b11111111111111,  // [8] 8
  0b11111111001111,  // [9] 9
  0b00000000000000,  // [10] off
  0b11111111000000,  // [11] degrees symbol
  0b00001111111100,  // [12] C(elsius)
  0b11001111110000,  // [13] F(ahrenheit)
};

void setup() {
  Serial.begin(115200); 
  delay(200);

/*!!! // RTC DS3231 Setup
  Rtc.Begin();    
  RtcDateTime compiled = RtcDateTime(__DATE__, __TIME__);

  if (!Rtc.IsDateTimeValid()) {
      if (Rtc.LastError() != 0) {  
          // We have a communications error see https://www.arduino.cc/en/Reference/WireEndTransmission for what the number means
          Serial.print("RTC communications error = ");
          Serial.println(Rtc.LastError());
      } else {
          // Common Causes:
          //    1) first time you ran and the device wasn't running yet
          //    2) the battery on the device is low or even missing
          Serial.println("RTC lost confidence in the DateTime!");
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
  FastLED.show();

  // WiFi - AP Mode or both
#if defined(WIFIMODE) && (WIFIMODE == 0 || WIFIMODE == 2) 
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP(APssid, APpassword);    // IP is usually 192.168.4.1
  Serial.println();
  Serial.print("SoftAP IP: ");
  Serial.println(WiFi.softAPIP());
#endif

  // WiFi - Local network Mode or both
#if defined(WIFIMODE) && (WIFIMODE == 1 || WIFIMODE == 2) 
  byte count = 0;
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    // Stop if cannot connect
    if (count >= 60) {
      Serial.println("Could not connect to local WiFi.");      
      return;
    }
    delay(500);
    Serial.print(".");
    timeClient.begin();
    count++;
  }
  Serial.print("Local IP: ");
  Serial.println(WiFi.localIP());
  IPAddress ip = WiFi.localIP();
  Serial.println(ip[3]);
#endif   

  httpUpdateServer.setup(&server);

  // Handlers
  server.on("/color", HTTP_POST, []() {    
    r_val = server.arg("r").toInt();
    g_val = server.arg("g").toInt();
    b_val = server.arg("b").toInt();
    autoChange = false;
    server.send(200, "text/json", "{\"result\":\"ok\"}");
  });

  server.on("/setdate", HTTP_POST, []() { 
    // Sample input: date = "Dec 06 2009", time = "12:34:56"
    // Jan Feb Mar Apr May Jun Jul Aug Sep Oct Nov Dec
    String datearg = server.arg("date");
    String timearg = server.arg("time");
    char d[12];
    char t[9];
    datearg.toCharArray(d, 12);
    timearg.toCharArray(t, 9);
//!!!    RtcDateTime compiled = RtcDateTime(d, t);
//!!!    Rtc.SetDateTime(compiled);   
    clockMode = 0;     
    server.send(200, "text/json", "{\"result\":\"ok\"}");
  });

  server.on("/brightness", HTTP_POST, []() {    
    brightness = server.arg("brightness").toInt();    
    server.send(200, "text/json", "{\"result\":\"ok\"}");
  });
  
  server.on("/countdown", HTTP_POST, []() {    
    countdownMilliSeconds = server.arg("ms").toInt();     
    byte cd_r_val = server.arg("r").toInt();
    byte cd_g_val = server.arg("g").toInt();
    byte cd_b_val = server.arg("b").toInt();
    countdownColor = CRGB(cd_r_val, cd_g_val, cd_b_val); 
    endCountDownMillis = millis() + countdownMilliSeconds;
    allBlank(); 
    clockMode = 1;     
    server.send(200, "text/json", "{\"result\":\"ok\"}");
  });

  server.on("/temperature", HTTP_POST, []() {   
    temperatureCorrection = server.arg("correction").toInt();
    temperatureSymbol = server.arg("symbol").toInt();
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
    last_digit = 55;      
    autoChange = true;
    server.send(200, "text/json", "{\"result\":\"ok\"}");
  });  

  server.serveStatic("/", SPIFFS, "/", "max-age=86400");
  server.begin();     
  SPIFFS.begin();
  colorNum = random(16);
  sensors.begin();
}

void loop(){
  server.handleClient(); 
  
  unsigned long currentMillis = millis();  
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

    FastLED.setBrightness(brightness);
    FastLED.show();
  }   
}

void displayNumber(byte number, byte segment, CRGB color) {
  byte startindex = 0;  // segment from left to right: 0, 1, 2, 3
  switch (segment) {
    case 3:
      startindex = 0;
      break;
    case 2:
      startindex = 14;
      break;
    case 1:
      startindex = 30;
      break;
    case 0:
      startindex = 44;
      break;    
  }

  for (byte i=0; i<14; i++){
    yield();
    LEDs[startindex + 13 - i] = ((numbers[number] & 1 << i) == 1 << i) ? color : CRGB::Black;
  } 
}

void allBlank() {
  for (int i=0; i<NUM_LEDS; i++) {
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

  byte h1 = hour / 10;
  byte h2 = hour % 10;
  byte m1 = mins / 10;
  byte m2 = mins % 10;  
  //byte s1 = secs / 10;
  //byte s2 = secs % 10;

  if ( autoChange ) {
     if (m2 != last_digit) {                    // Change color every minute
          color = ColorTable[colorNum++];
          if (colorNum >= 16) {
              colorNum = 0;
          }
          last_digit = m2;
      }
  }else 
      color = CRGB(r_val, g_val, b_val);    
  
  if (h1 > 0)
    displayNumber(h1,3,color);
  else 
    displayNumber(10,3,color);  // Blank
    
  displayNumber(h2,2,color);
  displayNumber(m1,1,color);
  displayNumber(m2,0,color); 
  displayDots(color);  
}

void updateCountdown() {

  if (countdownMilliSeconds == 0 && endCountDownMillis == 0) 
    return;
    
  unsigned long restMillis = endCountDownMillis - millis();
  unsigned long hours   = ((restMillis / 1000) / 60) / 60;
  unsigned long minutes = (restMillis / 1000) / 60;
  unsigned long seconds = restMillis / 1000;
  int remSeconds = seconds - (minutes * 60);
  int remMinutes = minutes - (hours * 60); 
  byte h1 = hours / 10;
  byte h2 = hours % 10;
  byte m1 = remMinutes / 10;
  byte m2 = remMinutes % 10;  
  byte s1 = remSeconds / 10;
  byte s2 = remSeconds % 10;

  CRGB color = countdownColor;
  if (restMillis <= 60000) {
    color = CRGB::Red;
  }

  if (hours > 0) {
    // hh:mm
    displayNumber(h1,3,color); 
    displayNumber(h2,2,color);
    displayNumber(m1,1,color);
    displayNumber(m2,0,color);  
  } else {
    // mm:ss   
    displayNumber(m1,3,color);
    displayNumber(m2,2,color);
    displayNumber(s1,1,color);
    displayNumber(s2,0,color);  
  }

  displayDots(color);  

  if (hours <= 0 && remMinutes <= 0 && remSeconds <= 0) {
    countdownMilliSeconds = 0;
    endCountDownMillis = 0;
    return;
  }  
}

void endCountdown() {
  allBlank();
  for (int i=0; i<NUM_LEDS; i++) {
    if (i>0)
      LEDs[i-1] = CRGB::Black;
    
    LEDs[i] = CRGB::Red;
    FastLED.show();
    delay(25);
  }  
}

void displayDots(CRGB color) {
  if (dotsOn) {
    LEDs[28] = color;
    LEDs[29] = color;
  } else 
      hideDots();

  dotsOn = !dotsOn;  
}

void hideDots() {
  LEDs[28] = CRGB::Black;
  LEDs[29] = CRGB::Black;
}

void updateTemperature() {
/*!!!  RtcTemperature temp = Rtc.GetTemperature();
  float ftemp = temp.AsFloatDegC();             // Sensor temp
  float ctemp = ftemp + temperatureCorrection;  // Corrected temp
!!!*/
  sensors.requestTemperatures();                // Get temp from DS18B20
  float tempC = sensors.getTempCByIndex(0); 

  if (temperatureSymbol == 13)
    tempC = (tempC * 1.8000) + 32;

  byte t1 = int(tempC) / 10;
  byte t2 = int(tempC) % 10;

  CRGB color = CRGB(r_val, g_val, b_val);
  displayNumber(t1,3,color);
  displayNumber(t2,2,color);
  displayNumber(11,1,color);
  displayNumber(temperatureSymbol,0,color);
  hideDots();
}

void updateScoreboard() {
  byte sl1 = scoreboardLeft / 10;
  byte sl2 = scoreboardLeft % 10;
  byte sr1 = scoreboardRight / 10;
  byte sr2 = scoreboardRight % 10;

  displayNumber(sl1,3,scoreboardColorLeft);
  displayNumber(sl2,2,scoreboardColorLeft);
  displayNumber(sr1,1,scoreboardColorRight);
  displayNumber(sr2,0,scoreboardColorRight);
  hideDots();
}
