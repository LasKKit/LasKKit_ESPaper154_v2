/* LasKKit ESPaper154 for Weather Station. 
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

// select the display class to use, on ly one
//GxEPD2_BW<GxEPD2_154 , GxEPD2_154::HEIGHT> display(GxEPD2_154(/*CS=D8*/ SS, /*DC=D3*/ 0, /*RST=D4*/ 2, /*BUSY=D2*/ 4)); // GDEP015OC1 no longer available
//GxEPD2_BW<GxEPD2_154_D67, GxEPD2_154_D67::HEIGHT> display(GxEPD2_154_D67(/*CS=D8*/ 5, /*DC=D3*/ 0, /*RST=D4*/ 2, /*BUSY=D2*/ 12)); // GDEH0154D67
// 3-color e-papers
GxEPD2_3C<GxEPD2_154c, GxEPD2_154c::HEIGHT> display(GxEPD2_154c(/*CS=D8*/ 5, /*DC=D3*/ 0, /*RST=D4*/ 2, /*BUSY=D2*/ 12));

#include <SPI.h>
#include <OneWire.h>
#include "ThingSpeak.h"
#include <ESP8266WiFi.h>
#include <DallasTemperature.h>

// date and time from NTP
#include <NTPClient.h>
#include <WiFiUdp.h>

#if defined(_GxGDEW0154Z04_H_) || defined(_GxGDEW0213Z16_H_) || defined(_GxGDEW029Z10_H_) || defined(_GxGDEW027C44_H_) || defined(_GxGDEW042Z15_H_) || defined(_GxGDEW075Z09_H_)
#define HAS_RED_COLOR
#endif

#define DSPIN 4

OneWire oneWireDS(DSPIN);
DallasTemperature dallas(&oneWireDS);
WiFiClient client;

// Konstanty pro vykresleni dlazdic
#define DLAZDICE_POSUN_Y 40
#define DLAZDICE_ODSAZENI_TEXT 5
#define DLAZDICE_ODSAZENI 10
#define DLAZDICE_VELIKOST_X 85
#define DLAZDICE_VELIKOST_Y 65

#define SLEEP_SEC 1*60  // Measurement interval (seconds)

float temp;
int pressure;
int humidity;
float m_volt;
float temp_in;
float d_volt = 3.7;
int32_t wifiSignal;
String date;

WiFiUDP ntpUDP;
// Secify the time server pool and the offset, (+3600 in seconds, GMT +1 hour)
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
  m_volt = round(m_volt*10.0)/10.0;
  Serial.print("Meteo Battery voltage: ");
  Serial.println(m_volt);
     //---------------- Channel 5 ----------------//
  float temp_box = ThingSpeak.readFloatField(myChannelNumber, 5, myReadAPIKey);
  Serial.print("Temperature inside: ");
  Serial.println(temp_box);
}


/*
   Funkce pro ziskani rozmeru hypotetickeho vykresleneho textu
   Pouziji ji pro vypocet pozice kurzoru pro centrovani na stred
*/
void ziskejRozmery(char text[], uint16_t* sirka, uint16_t* vyska) {
  int16_t  x1, y1;
  display.getTextBounds(text, 0, 0, &x1, &y1, sirka, vyska);
}
void ziskejRozmery(String text, uint16_t* sirka, uint16_t* vyska) {
  int16_t  x1, y1;
  display.getTextBounds(text, 0, 0, &x1, &y1, sirka, vyska);
}

// Samotna funkce pro vykresleni barevne dlazdice
void nakresliDlazdici(uint8_t pozice, char nadpis[], char hodnota[]) {
  // Souradnie dlazdice
  uint16_t x = 0;
  uint16_t y = 0;

  // Souradnice dlazdice podle jedne ze ctyr moznych pozic (0 az 3)
  switch (pozice) {
    case 0:
      x = DLAZDICE_ODSAZENI;
      y = DLAZDICE_POSUN_Y;
      break;
    case 1:
      x = (DLAZDICE_ODSAZENI * 2) + DLAZDICE_VELIKOST_X;
      y = DLAZDICE_POSUN_Y;
      break;
    case 2:
      x = DLAZDICE_ODSAZENI;
      y = DLAZDICE_POSUN_Y + DLAZDICE_VELIKOST_Y + DLAZDICE_ODSAZENI;
      break;
    case 3:
      x = (DLAZDICE_ODSAZENI * 2) + DLAZDICE_VELIKOST_X;
      y = DLAZDICE_POSUN_Y + DLAZDICE_VELIKOST_Y + DLAZDICE_ODSAZENI;
      break;
  }

  // Vykresleni stinu a dlazdice
  //display.drawRect(x + 1, y + 1, DLAZDICE_VELIKOST_X, DLAZDICE_VELIKOST_Y, GxEPD_BLACK);

  display.drawRect(x, y, DLAZDICE_VELIKOST_X, DLAZDICE_VELIKOST_Y, GxEPD_BLACK);

  // Vycentrovani a vykresleni nadpisu dlazdice
  display.setFont(&FreeMono9pt7b);
  uint16_t sirka, vyska;
  ziskejRozmery(nadpis, &sirka, &vyska);
  display.setCursor(x + ((DLAZDICE_VELIKOST_X / 2) - (sirka / 2)),
                    y + DLAZDICE_ODSAZENI_TEXT + vyska);
  display.print(nadpis);
  
  // Vycentrovani a vykresleni hlavni hodnoty
  display.setFont(&FreeMonoBold12pt7b);
  ziskejRozmery(hodnota, &sirka, &vyska);
  display.setCursor(x + ((DLAZDICE_VELIKOST_X / 2) - (sirka / 2)),
                    y + ((DLAZDICE_VELIKOST_Y / 2) + vyska));
  
  #if defined(HAS_RED_COLOR)
  display.setTextColor(GxEPD_RED);
  #endif

  display.print(hodnota);

  display.setTextColor(GxEPD_BLACK);
}

/* Pomocne pretizene funkce pro rozliseni, jestli se jedna o blok
   s promennou celeho cisla, nebo cisla s desetinou carkou
*/
void nakresliDlazdici(uint8_t pozice, char nadpis[], float hodnota) {
  // Prevod ciselne hodnoty float na retezec
  char strHodnota[8];
  dtostrf(hodnota, 3, 1, strHodnota);
  nakresliDlazdici(pozice, nadpis, strHodnota);
}

void nakresliDlazdici(uint8_t pozice, char nadpis[], int hodnota) {
  // Prevod ciselne hodnoty int na retezec
  char strHodnota[8];
  itoa(hodnota, strHodnota, 10);
  nakresliDlazdici(pozice, nadpis, strHodnota);
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
  // d_volt = analogRead(A0) / 1023.0 * 4.24;

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
  display.setRotation(0);
  display.setFont(&FreeMono9pt7b);
  display.setTextColor(GxEPD_BLACK);
  display.setFullWindow();
  display.fillScreen(GxEPD_WHITE);

  // logo laskarduino
 //display.drawBitmap(laskarduino_glcd_bmp, 200/2-24, 200/2-24, 48, 48, GxEPD_BLACK, 0);

  // WiFi signal
  int32_t wifiSignalMax = 4;
  int32_t offcet = 6;
  
  display.drawBitmap(0, 0, wifi1_icon16x16, 16, 16, GxEPD_BLACK, 0);  
  for (int32_t i = 1; i <= wifiSignalMax; i++)
      display.drawRect(i * offcet - 6 + 18, 0, 4, 13, GxEPD_BLACK);

  for (int32_t i = 1; i <= wifiSignal; i++)
      display.fillRect(i * offcet - 6+18, 0, 4, 13, GxEPD_BLACK);

  // Napeti baterie meteostanice
  uint16_t sirka, vyska;
  String meteoBateryVoltage = "";
  meteoBateryVoltage = String(m_volt,2)  + "v";
  ziskejRozmery(meteoBateryVoltage, &sirka, &vyska);
  display.setCursor(100 - (sirka / 2), vyska);
  display.print(meteoBateryVoltage);

  // Napeti baterie
  uint8_t intBatteryPercentage = getIntBattery();
  switch (intBatteryPercentage) {
    case 5:
    display.drawBitmap(200-27, 0, bat_100, 27, 16, GxEPD_BLACK, 0);
      break;
     case 4:
    display.drawBitmap(200-27, 0, bat_80, 27, 16, GxEPD_BLACK, 0);
      break;
    case 3:
    display.drawBitmap(200-27, 0, bat_60, 27, 16, GxEPD_BLACK, 0);
      break;
    case 2:
    display.drawBitmap(200-27, 0, bat_40, 27, 16, GxEPD_BLACK, 0);
      break;
     case 1:
    display.drawBitmap(200-27, 0, bat_20, 27, 16, GxEPD_BLACK, 0);
      break;
    case 0:
    display.drawBitmap(200-27, 0, bat_0, 27, 16, GxEPD_BLACK, 0);
      break;
    default:
    break;
  }

  // datum a cas posledni aktualizace
  date = "upd:" + date;
  ziskejRozmery(date, &sirka, &vyska);
  display.setCursor(100 - (sirka / 2), vyska + 18);
  display.print(date);

  //draw squares
  nakresliDlazdici(0, "Tout,`C", temp);
  nakresliDlazdici(1, "Vlh,%", humidity);
  nakresliDlazdici(2, "Tl,hPa", pressure);
  nakresliDlazdici(3, "Tin,`C", temp_in);
  
  display.display(false);  // full update
}

void WiFiConnection(){
  // pripojeni k WiFi
  Serial.println();
  Serial.print("Connecting to...");
  Serial.println(ssid);

 // WiFi.config(ip,gateway,subnet);
  WiFi.begin(ssid, pass);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("Wi-Fi connected successfully");
}

void setup() {

  // disable WiFi, coming from DeepSleep, as we do not need it right away
  WiFi.mode( WIFI_OFF );
  WiFi.forceSleepBegin();
  delay( 1 );

  Serial.begin(115200);
  while(!Serial) {} // Wait until serial is ok
  
  display.init(9600); // enable diagnostic output on Serial
  
  // initilizace DS18B20
  dallas.begin();
  dallas.requestTemperatures();
  temp_in = dallas.getTempCByIndex(0); // (x) - pořadí dle unikátní adresy čidel
  Serial.print("Temp_in: "); Serial.print(temp_in); Serial.println(" °C");

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

  // ESP Sleep
  Serial.println("ESP8266 in sleep mode");
  ESP.deepSleep(SLEEP_SEC * 1000000, WAKE_RF_DISABLED); 
}

void loop() {
  // Generally we dont use the loop
}