#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <U8g2lib.h>
#include <SPI.h>
#include "time.h"
#include "secrets.h"

// secrets.h 설정 값
const char* ssid     = SECRET_SSID;
const char* password = SECRET_PASSWORD;
const char* apiKey   = SECRET_API_KEY;

// 대한민국 시간 설정 (UTC+9)
const long  gmtOffset_sec = 32400;
const int   daylightOffset_sec = 0; 
const char* ntpServer = "pool.ntp.org";

// ST7567A 하드웨어 SPI 디스플레이 설정
U8G2_ST7567_OS12864_F_4W_HW_SPI u8g2(U8G2_R0, /* cs=*/ 5, /* dc=*/ 21, /* reset=*/ 22);

// [애니메이션 모드 선택] 1: 팩맨 | 2: 고양이 | 3: 하트
const int ANIMATION_MODE = 1; 

// 전역 변수
unsigned long lastWeatherCheck = 0;
const unsigned long weatherInterval = 600000; 

String currentCity = "Seoul";   
String weatherMain = "---";     
float temperature = 0.0;        
int humidity = 0;               

int iconX = 6;           
int iconDirection = 1;    
unsigned long lastIconUpdate = 0;
const unsigned long iconInterval = 50; 
int animFrame = 0;        

// 하위 함수 전방 선언
void getCurrentCity();
void getWeatherData();
void handleAnimation();

// -------------------------------------------------------------
// UI 전용 함수
// -------------------------------------------------------------
void drawIntroLogo() {
  const char* introLogo = "DASOMBANG";
  u8g2.setFont(u8g2_font_courB10_tf);
  u8g2.drawStr((128 - u8g2.getStrWidth(introLogo)) / 2, 28, introLogo);
}

void updateStatus(const char* msg, int frameStep) {
  u8g2.clearBuffer();
  drawIntroLogo(); 
  
  // 기존 u8g2_font_6x10_tf에서 5x8_tf로 변경
  u8g2.setFont(u8g2_font_5x8_tf); 
  
  int msgWidth = u8g2.getStrWidth(msg);
  int msgX = (128 - (msgWidth + 24)) / 2;
  u8g2.drawStr(msgX, 52, msg);
  
  for (int i = 0; i < frameStep; i++) {
    u8g2.drawStr(msgX + msgWidth + 2 + (i * 4), 52, ".");
  }
  u8g2.sendBuffer();
}

// -------------------------------------------------------------
// 주요 로직 함수
// -------------------------------------------------------------
void connectWiFi() {
  WiFi.begin(ssid, password);
  int dotCount = 0;
  unsigned long lastUpdate = 0;
  
  while (WiFi.status() != WL_CONNECTED) {
    if (millis() - lastUpdate >= 300) {
      updateStatus("Connecting to WiFi", dotCount);
      dotCount = (dotCount + 1) % 4; 
      lastUpdate = millis();
    }
    delay(10);
  }
}

void getCurrentCity() {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    http.begin("http://ip-api.com/json/?lang=en"); 
    
    int dotCount = 0;
    for (int i = 0; i < 7; i++) {
      updateStatus("Finding City", dotCount);
      dotCount = (dotCount + 1) % 4;
      delay(300);
    }

    int httpCode = http.GET();
    if (httpCode == HTTP_CODE_OK) {
      String payload = http.getString();
      JsonDocument doc;
      DeserializationError error = deserializeJson(doc, payload);
      if (!error) {
        String status = doc["status"].as<String>();
        if (status == "success") {
          currentCity = doc["city"].as<String>();
          return;
        }
      }
    }
    currentCity = "Seoul"; 
  }
}

void getWeatherData() {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    String url = "http://api.openweathermap.org/data/2.5/weather?q=" + currentCity + "&appid=" + String(apiKey) + "&units=metric";
    http.begin(url);

    int dotCount = 0;
    for (int i = 0; i < 7; i++) {
      updateStatus("Loading Weather", dotCount);
      dotCount = (dotCount + 1) % 4;
      delay(300);
    }

    int httpCode = http.GET();
    if (httpCode == HTTP_CODE_OK) { 
      String payload = http.getString();
      JsonDocument doc;
      DeserializationError error = deserializeJson(doc, payload);
      if (!error) {
        weatherMain = doc["weather"][0]["main"].as<String>();
        temperature = doc["main"]["temp"].as<float>();
        humidity = doc["main"]["humidity"].as<int>();
      }
    }
    http.end();
  }
}

// -------------------------------------------------------------
// 애니메이션 함수셋
// -------------------------------------------------------------
void drawPacman(int x, int dir) {
  int baseY = 32;
  if (dir == 1) {
    for (int dotX = 12; dotX < 120; dotX += 12) {
      if (dotX > x + 3) { u8g2.drawPixel(dotX, baseY); }
    }
    u8g2.drawDisc(x, baseY, 3);
    u8g2.setDrawColor(0);
    u8g2.drawTriangle(x, baseY, x + 4, baseY - 2, x + 4, baseY + 2); 
    u8g2.setDrawColor(1);
  } else {
    u8g2.drawDisc(x, baseY, 3);
    u8g2.setDrawColor(0);
    u8g2.drawTriangle(x, baseY, x - 4, baseY - 2, x - 4, baseY + 2); 
    u8g2.setDrawColor(1);

    int ghostX = x + 16; 
    u8g2.drawBox(ghostX - 2, baseY - 1, 5, 4);      
    u8g2.drawPixel(ghostX - 1, baseY - 2);          
    u8g2.drawPixel(ghostX, baseY - 2);
    u8g2.drawPixel(ghostX + 1, baseY - 2);
    u8g2.drawBox(ghostX - 3, baseY, 7, 4);
    if (animFrame % 2 == 0) {
      u8g2.drawPixel(ghostX - 2, baseY + 3);
      u8g2.drawPixel(ghostX, baseY + 3);
      u8g2.drawPixel(ghostX + 2, baseY + 3);
    } else {
      u8g2.drawPixel(ghostX - 1, baseY + 3);
      u8g2.drawPixel(ghostX + 1, baseY + 3);
    }
  }
}

void drawCat(int x, int dir) {
  int baseY = 32;
  if (dir == 1) {
    u8g2.drawBox(x - 3, baseY - 1, 5, 3); u8g2.drawBox(x + 2, baseY - 3, 3, 3); 
    u8g2.drawPixel(x + 2, baseY - 4); u8g2.drawPixel(x + 4, baseY - 4); u8g2.drawVLine(x - 4, baseY - 3, 3); 
    if (animFrame % 2 == 0) { u8g2.drawPixel(x - 2, baseY + 2); u8g2.drawPixel(x + 1, baseY + 2); } 
    else { u8g2.drawPixel(x - 3, baseY + 2); u8g2.drawPixel(x + 2, baseY + 2); }
  } else {
    u8g2.drawBox(x - 2, baseY - 1, 5, 3); u8g2.drawBox(x - 5, baseY - 3, 3, 3); 
    u8g2.drawPixel(x - 5, baseY - 4); u8g2.drawPixel(x - 3, baseY - 4); u8g2.drawVLine(x + 3, baseY - 3, 3); 
    if (animFrame % 2 == 0) { u8g2.drawPixel(x - 3, baseY + 2); u8g2.drawPixel(x + 1, baseY + 2); } 
    else { u8g2.drawPixel(x - 4, baseY + 2); u8g2.drawPixel(x, baseY + 2); }
  }
}

void drawBouncingHeart(int x) {
  int bounceY = 31 + (int)(sin(x * 0.2) * 3);
  u8g2.drawPixel(x - 1, bounceY - 2); u8g2.drawPixel(x + 1, bounceY - 2);
  u8g2.drawBox(x - 2, bounceY - 1, 5, 2); u8g2.drawBox(x - 1, bounceY + 1, 3, 1); u8g2.drawPixel(x, bounceY + 2);
}

void handleAnimation() {
  iconX += iconDirection; animFrame++; 
  int maxRight = (ANIMATION_MODE == 1) ? 100 : 114; int minLeft = 10; 
  if (iconX >= maxRight) { iconX = maxRight; iconDirection = -1; } 
  else if (iconX <= minLeft) { iconX = minLeft; iconDirection = 1; } 
  switch (ANIMATION_MODE) { 
    case 1: drawPacman(iconX, iconDirection); break; 
    case 2: drawCat(iconX, iconDirection); break; 
    case 3: drawBouncingHeart(iconX); break; 
    default: u8g2.drawHLine(iconX - 4, 32, 8); break; 
  }
}

// -------------------------------------------------------------
// 메인 디스플레이 출력 함수 (도씨 기호는 \xb0 코드로 안전 처리)
// -------------------------------------------------------------
void displayAllInfo(struct tm timeinfo) {
  u8g2.clearBuffer(); 
  char dateTimeBuffer[30]; 
  sprintf(dateTimeBuffer, "%02d-%02d  %02d:%02d:%02d", 
          timeinfo.tm_mon + 1, timeinfo.tm_mday, 
          timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
  u8g2.setFont(u8g2_font_7x14B_mn);  
  u8g2.drawStr((128 - u8g2.getStrWidth(dateTimeBuffer)) / 2, 20, dateTimeBuffer);

  handleAnimation();

  u8g2.setFont(u8g2_font_6x10_tf); 
  u8g2.drawStr((128 - u8g2.getStrWidth(currentCity.c_str())) / 2, 44, currentCity.c_str()); 

  char weatherBuffer[35]; 
  sprintf(weatherBuffer, "%s  %.1f\xb0" "C  %d%%", weatherMain.c_str(), temperature, humidity); 
  
  u8g2.setFont(u8g2_font_6x10_tf); 
  u8g2.drawStr((128 - u8g2.getStrWidth(weatherBuffer)) / 2, 58, weatherBuffer); 
  u8g2.sendBuffer();
}

void setup() {
  Serial.begin(115200);
  u8g2.begin();  
  connectWiFi();        
  getCurrentCity();     
  configTime(32400, 0, "pool.ntp.org"); 
  getWeatherData();     
}

void loop() {
  unsigned long currentMillis = millis(); 
  if (currentMillis - lastWeatherCheck >= weatherInterval) {
    lastWeatherCheck = currentMillis;
    getWeatherData();
  } 
  struct tm timeinfo; 
  if (getLocalTime(&timeinfo)) { 
    if (currentMillis - lastIconUpdate >= iconInterval) {
      lastIconUpdate = currentMillis;
      displayAllInfo(timeinfo); 
    }
  }
}