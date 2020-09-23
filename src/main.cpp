/* LasKKit ESPaper154 V2 for Weather Station. 
 * Thingspeak edition
 * Read Temperature, Humidity and pressure from Thingspeak and show on E-Paper display
 * For settings see config.h
 * 
 * Email:obchod@laskarduino.cz
 * Web:laskarduino.cz
 * 
 * Miles Burton DS18B20 library
 * https://github.com/milesburton/Arduino-Temperature-Control-Library
 */

// mapping suggestion from Waveshare SPI e-Paper to generic ESP8266
// BUSY -> GPIO4, RST -> GPIO2, DC -> GPIO0, CS -> GPIO15, CLK -> GPIO14, DIN -> GPIO13, GND -> GND, 3.3V -> 3.3V
// NOTE: connect 4.7k pull-down from GPIO15 to GND if your board or shield has level converters
// NOTE for ESP8266: using SS (GPIO15) for CS may cause boot mode problems, use different pin in case, or 4k7 pull-down

// include library, include base class, make path known
#include <Arduino.h>
#include "config_my.h"        // change to config.h and fill the file.
#include "iot_iconset_16x16.h"

// E-paper
// base class GxEPD2_GFX can be used to pass references or pointers to the display instance as parameter, uses ~1.2k more code
// enable or disable GxEPD2_GFX base class
#define ENABLE_GxEPD2_GFX 0
#include <GxEPD2_BW.h>
#include <GxEPD2_3C.h>

#include "FreeMono9pt7b.h"
// FreeFonts from Adafruit_GFX
#include <Fonts/FreeMonoBold12pt7b.h>

// select the display class to use, only one
GxEPD2_BW<GxEPD2_154 , GxEPD2_154::HEIGHT> display(GxEPD2_154(/*CS=D8*/ 5, /*DC=D3*/ 0, /*RST=D4*/ 2, /*BUSY=D2*/ 4)); // GDEP015OC1 no longer available (Waveshare V2)
//GxEPD2_BW<GxEPD2_154_D67, GxEPD2_154_D67::HEIGHT> display(GxEPD2_154_D67(/*CS=D8*/ SS, /*DC=D3*/ 0, /*RST=D4*/ 2, /*BUSY=D2*/ 4)); // GDEH0154D67

// 3-color e-papers
//GxEPD2_3C<GxEPD2_154c, GxEPD2_154c::HEIGHT> display(GxEPD2_154c(/*CS=D8*/ SS, /*DC=D3*/ 0, /*RST=D4*/ 2, /*BUSY=D2*/ 4));
//#define HAS_RED_COLOR

#include <SPI.h>
#include "ThingSpeak.h"
#include <ESP8266WiFi.h>

// date and time from NTP
#include <NTPClient.h>
#include <WiFiUdp.h>

#define USE_DALLAS 0

#if USE_DALLAS
#include <OneWire.h>
#include <DallasTemperature.h>
#define DSPIN 12
OneWire oneWireDS(DSPIN);
DallasTemperature dallas(&oneWireDS);
#endif

WiFiClient client;

IPAddress ip(192,168,100,244);       // pick your own IP outside the DHCP range of your router
IPAddress gateway(192,168,100,1);   // watch out, these are comma's not dots
IPAddress subnet(255,255,255,0);

// Konstanty pro vykresleni dlazdic
#define TILE_SHIFT_Y 40
#define TEXT_PADDING 5      // odsazeni text
#define TILE_MARGIN 10      // odsazeni dlazdicka
#define TILE_SIZE_X 85
#define TILE_SIZE_Y 65

#define SLEEP_SEC 1*60  // Measurement interval (seconds)

float temp;
int pressure;
int humidity;
float m_volt;
float temp_in;
float d_volt = 3.7;
int32_t wifiSignal;
String date;

unsigned int timeAwake;

WiFiUDP ntpUDP;
// Secify the time server pool and the offset, (+3600 in seconds, GMT +1 hour)
// !!!!!!!!!!!!!!!!!!!!!!TODO WRONG SUMMER TIME (- 1 HOUR)
// additionaly you can specify the update interval (in milliseconds). 
NTPClient timeClient(ntpUDP, "europe.pool.ntp.org", 3600, 60000);

void readChannel(){
   //---------------- Channel 1 ----------------//
  temp = ThingSpeak.readFloatField(myChannelNumber, 1, myReadAPIKey);
  Serial.print("Temperature: ");
  Serial.println(temp);
  //---------------- Channel 2 ----------------//
  pressure = ThingSpeak.readIntField(myChannelNumber, 2, myReadAPIKey);
  Serial.print("Pressure: ");
  Serial.println(pressure);
   //---------------- Channel 3 ----------------//
  humidity = ThingSpeak.readIntField(myChannelNumber, 3, myReadAPIKey);
  Serial.print("Humidity: ");
  Serial.println(humidity);
   //---------------- Channel 4 ----------------//
  m_volt = ThingSpeak.readFloatField(myChannelNumber, 4, myReadAPIKey);
  m_volt = round(m_volt*100.0)/100.0;     // round to x,xx
  Serial.print("Meteo Battery voltage: ");
  Serial.println(m_volt);
     //---------------- Channel 5 ----------------//
  float temp_box = ThingSpeak.readFloatField(myChannelNumber, 5, myReadAPIKey);
  Serial.print("Temperature inside: ");
  Serial.println(temp_box);
}

/*
   Funkce pro ziskani rozmeru hypotetickeho vykresleneho textu
   Pouziji ji pro vypocet position kurzoru pro centrovani na stred
*/
void getDimensions(char text[], uint16_t* width, uint16_t* height) {
  int16_t  x1, y1;
  display.getTextBounds(text, 0, 0, &x1, &y1, width, height);
}
void getDimensions(String text, uint16_t* width, uint16_t* height) {
  int16_t  x1, y1;
  display.getTextBounds(text, 0, 0, &x1, &y1, width, height);
}

// Samotna funkce pro vykresleni barevne dlazdice
void drawTile(uint8_t position, char title[], char value[]) {
  // Souradnie dlazdice
  uint16_t x = 0;
  uint16_t y = 0;

  // Souradnice dlazdice podle jedne ze ctyr moznych pozic (0 az 3)
  switch (position) {
    case 0:
      x = TILE_MARGIN;
      y = TILE_SHIFT_Y;
      break;
    case 1:
      x = (TILE_MARGIN * 2) + TILE_SIZE_X;
      y = TILE_SHIFT_Y;
      break;
    case 2:
      x = TILE_MARGIN;
      y = TILE_SHIFT_Y + TILE_SIZE_Y + TILE_MARGIN;
      break;
    case 3:
      x = (TILE_MARGIN * 2) + TILE_SIZE_X;
      y = TILE_SHIFT_Y + TILE_SIZE_Y + TILE_MARGIN;
      break;
  }

  // Vykresleni stinu a dlazdice
  //display.drawRect(x + 1, y + 1, TILE_SIZE_X, TILE_SIZE_Y, GxEPD_BLACK);

  display.drawRect(x, y, TILE_SIZE_X, TILE_SIZE_Y, GxEPD_BLACK);

  // Vycentrovani a vykresleni titleu dlazdice
  display.setFont(&FreeMono9pt7b);
  uint16_t width, height;
  getDimensions(title, &width, &height);
  display.setCursor(x + ((TILE_SIZE_X / 2) - (width / 2)),
                    y + TEXT_PADDING + height);
  display.print(title);
  
  // Vycentrovani a vykresleni hlavni hodnoty
  display.setFont(&FreeMonoBold12pt7b);
  getDimensions(value, &width, &height);
  display.setCursor(x + ((TILE_SIZE_X / 2) - (width / 2)),
                    y + ((TILE_SIZE_Y / 2) + height));
  
  #if defined(HAS_RED_COLOR)
  display.setTextColor(GxEPD_RED);
  #endif

  display.print(value);
  display.setTextColor(GxEPD_BLACK);
}

/* Pomocne pretizene funkce pro rozliseni, jestli se jedna o blok
   s promennou celeho cisla, nebo cisla s desetinou carkou
*/
void drawTile(uint8_t position, char title[], float value) {
  // Prevod ciselne hodnoty float na retezec
  char strvalue[8];
  dtostrf(value, 3, 1, strvalue);
  drawTile(position, title, strvalue);
}

void drawTile(uint8_t position, char title[], int value) {
  // Prevod ciselne hodnoty int na retezec
  char strvalue[8];
  itoa(value, strvalue, 10);
  drawTile(position, title, strvalue);
}

uint8_t getWifiStrength(){
  int32_t strength = WiFi.RSSI();
  Serial.print("Wifi Strenght: " + String(strength) + "dB; ");

  uint8_t percentage;
  if(strength <= -100) {
    percentage = 0;
  } else if(strength >= -50) {  
    percentage = 100;
  } else {
    percentage = 2 * (strength + 100);
  }
  Serial.println(String(percentage) + "%");  //Signal strength in %  

  if (percentage >= 75) strength = 4;
  else if (percentage >= 50 && percentage < 75) strength = 3;
  else if (percentage >= 25 && percentage < 50) strength = 2;
  else if (percentage >= 10 && percentage < 25) strength = 1;
  else strength = 0;
  return strength;
}

uint8_t getIntBattery(){
  d_volt = analogRead(A0);
  Serial.println(d_volt);
  if (d_volt > 0) {
    d_volt = analogRead(A0) / 1023.0 * 4.24;
    Serial.println(String(d_volt) + "V");
  }

  // Simple percentage converting
  if (d_volt >= 4.0) return 5;
  else if (d_volt >= 3.8 && d_volt < 4.0) return 4;
  else if (d_volt >= 3.73 && d_volt < 3.8) return 3;
  else if (d_volt >= 3.65 && d_volt < 3.73) return 2;
  else if (d_volt >= 3.6 && d_volt < 3.65) return 1;
  else if (d_volt < 3.6) return 0;
  else return 0;
}

String getTime(){
  while(!timeClient.update()) {
    timeClient.forceUpdate();
  }

  String formattedDate = timeClient.getFormattedDate();

  // Extract date
  int splitT = formattedDate.indexOf("T");
  
  String dayStamp = formattedDate.substring(0, splitT);
  String day = dayStamp.substring(8, 10);
  String month = dayStamp.substring(5, 7);
  
  //Extract time
  String timeStamp = formattedDate.substring(splitT+1, formattedDate.length()-1);
  String hour = timeStamp.substring(0, 5);
  return day + "." + month + " " + hour;
}

void drawScreen() {
  timeAwake = millis();
  display.setRotation(0);
  display.setFont(&FreeMono9pt7b);
  display.setTextColor(GxEPD_BLACK);
  display.setFullWindow();
  display.fillScreen(GxEPD_WHITE);

  // logo laskarduino
 //display.drawBitmap(laskarduino_glcd_bmp, 200/2-24, 200/2-24, 48, 48, GxEPD_BLACK, GxEPD_WHITE);

  // WiFi signal
  int32_t wifiSignalMax = 4;
  int32_t offcet = 6;
  
  display.drawBitmap(0, 0, wifi1_icon16x16, 16, 16, GxEPD_BLACK, GxEPD_WHITE);  
  for (int32_t i = 1; i <= wifiSignalMax; i++)
      display.drawRect(i * offcet - 6 + 18, 0, 4, 13, GxEPD_BLACK);

  for (int32_t i = 1; i <= wifiSignal; i++)
      display.fillRect(i * offcet - 6+18, 0, 4, 13, GxEPD_BLACK);

  // Napeti baterie meteostanice
  uint16_t width, height;
  String meteoBateryVoltage = "";
  meteoBateryVoltage = String(m_volt,2)  + "v";
  getDimensions(meteoBateryVoltage, &width, &height);
  display.setCursor(100 - (width / 2), height);
  display.print(meteoBateryVoltage);

  // Napeti baterie
  uint8_t intBatteryPercentage = getIntBattery();
  switch (intBatteryPercentage) {
    case 5:
    display.drawBitmap(200-27, 0, bat_100, 27, 16, GxEPD_BLACK, GxEPD_WHITE);
      break;
     case 4:
    display.drawBitmap(200-27, 0, bat_80, 27, 16, GxEPD_BLACK, GxEPD_WHITE);
      break;
    case 3:
    display.drawBitmap(200-27, 0, bat_60, 27, 16, GxEPD_BLACK, GxEPD_WHITE);
      break;
    case 2:
    display.drawBitmap(200-27, 0, bat_40, 27, 16, GxEPD_BLACK, GxEPD_WHITE);
      break;
     case 1:
    display.drawBitmap(200-27, 0, bat_20, 27, 16, GxEPD_BLACK, GxEPD_WHITE);
      break;
    case 0:
    display.drawBitmap(200-27, 0, bat_0, 27, 16, GxEPD_BLACK, GxEPD_WHITE);
      break;
    default:
    break;
  }

  // datum a cas posledni aktualizace
  date = "upd:" + date;
  getDimensions(date, &width, &height);
  display.setCursor(100 - (width / 2), height + 18);
  display.print(date);

  //draw squares
  drawTile(0, "Tout,`C", temp);
  drawTile(1, "Vlh,%", humidity);
  drawTile(2, "Tl,hPa", pressure);
  drawTile(3, "Tin,`C", temp_in);
  
  display.display(false);  // full update
  timeAwake = millis() - timeAwake;
  Serial.print("-Time for display update: ");  Serial.print(timeAwake); Serial.println(" ms");
}

void WiFiConnection(){
  timeAwake = millis();
  // pripojeni k WiFi
  Serial.println();
  Serial.print("Connecting to...");
  Serial.println(ssid);

  //WiFi.config(ip,gateway,subnet);   // Použít statickou IP adresu, config.h | Use static ip address
  WiFi.begin(ssid, pass);

  int i = 0;
  while (WiFi.status() != WL_CONNECTED) {
    delay(100);
    i++;
    if (i == 10) {
      i = 0;
      Serial.println(".");
    } else Serial.print("."); 
  }
  Serial.println("");
  Serial.println("Wi-Fi connected successfully");
  timeAwake = millis() - timeAwake;
  Serial.print("-Time for connecting: ");  Serial.print(timeAwake); Serial.println(" ms");
}

void setup() {

  // disable WiFi, coming from DeepSleep, as we do not need it right away
  WiFi.mode( WIFI_OFF );
  WiFi.forceSleepBegin();
  delay( 1 );

  Serial.begin(9600);
  while(!Serial) {} // Wait until serial is ok
  
  display.init(9600); // enable diagnostic output on Serial
  
  #if USE_DALLAS
    // initilizace DS18B20
    dallas.begin();
    dallas.requestTemperatures();
    temp_in = dallas.getTempCByIndex(0); // (x) - pořadí dle unikátní adresy čidel
    Serial.print("Temp_in: "); Serial.print(temp_in); Serial.println(" °C");
  #else
    temp_in = 0;
    Serial.print("No dallas was defined");
  #endif


  // pripojeni k WiFi
  //Switch Radio back On
  WiFi.forceSleepWake();
  delay( 1 );
  // Bring up the WiFi connection
  WiFi.mode( WIFI_STA );

  WiFiConnection();

  wifiSignal = getWifiStrength();
  date = getTime();

  ThingSpeak.begin(client);
  timeClient.begin();

  readChannel();

  WiFi.disconnect(true);
  delay(1);

  drawScreen();

  // turns off generation of panel driving voltages, avoids screen fading over time
  display.powerOff();

  Serial.print("-Time Awake: ");  Serial.print(millis()); Serial.println(" ms");

  // ESP Sleep
  Serial.println("ESP8266 in sleep mode");
  ESP.deepSleep(SLEEP_SEC * 1000000, WAKE_RF_DISABLED); 
}

void loop() {
  // Generally we dont use the loop
}