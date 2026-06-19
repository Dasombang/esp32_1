#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <U8g2lib.h>
#include <SPI.h>
#include "time.h"
#include "secrets.h"

// secrets.h에서 설정 값 가져오기
const char* ssid     = SECRET_SSID;
const char* password = SECRET_PASSWORD;
const char* apiKey   = SECRET_API_KEY;

// 대한민국 시간 설정 (UTC+9)
const long  gmtOffset_sec = 32400;
const int   daylightOffset_sec = 0; 
const char* ntpServer = "pool.ntp.org";

// ST7567A 하드웨어 SPI 디스플레이 설정
U8G2_ST7567_OS12864_F_4W_HW_SPI u8g2(U8G2_R0, /* cs=*/ 5, /* dc=*/ 21, /* reset=*/ 22);

// -------------------------------------------------------------
// [애니메이션 모드 선택] 1: 팩맨과 유령 | 2: 아장아장 고양이 | 3: 통통 하트
// -------------------------------------------------------------
const int ANIMATION_MODE = 1; 

// 전역 변수
int lastSecond = -1;
unsigned long lastWeatherCheck = 0;
const unsigned long weatherInterval = 600000; 

String currentCity = "Seoul";   
String weatherMain = "---";     
float temperature = 0.0;        
int humidity = 0;               

// 애니메이션 제어 변수 (여러 마리가 등장하므로 이동 가능 범위를 연장)
int iconX = 6;           
int iconDirection = 1;    
unsigned long lastIconUpdate = 0;
const unsigned long iconInterval = 50; // 50ms (0.05초)마다 움직임 갱신
int animFrame = 0;        

void connectWiFi() {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.drawStr(0, 15, "Connecting to WiFi...");
  u8g2.sendBuffer();

  WiFi.begin(ssid, password);
  int x_offset = 0;
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    u8g2.drawStr(x_offset, 35, ".");
    u8g2.sendBuffer();
    x_offset += 6;
    if (x_offset > 120) {
      u8g2.setDrawColor(0);
      u8g2.drawBox(0, 25, 128, 20);
      u8g2.setDrawColor(1);
      x_offset = 0;
    }
  }
}

void getCurrentCity() {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    http.begin("http://ip-api.com/json/?lang=en"); 
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
// [수정 완료] 팩맨 도망 시 입 벌림 적용 함수
// -------------------------------------------------------------
void drawPacman(int x, int dir) {
  int baseY = 33;
  
  if (dir == 1) {
    // ➡️ 1단계: 우측 이동 (먹이 먹기)
    
    // 먹이 점 배치
    for (int dotX = 12; dotX < 120; dotX += 12) {
      if (dotX > x + 3) { 
        u8g2.drawPixel(dotX, baseY);
      }
    }

    // 팩맨 본체 (우측 입)
    u8g2.drawDisc(x, baseY, 3);
    u8g2.setDrawColor(0);
    u8g2.drawTriangle(x, baseY, x + 4, baseY - 2, x + 4, baseY + 2); 
    u8g2.setDrawColor(1);

  } else {
    // ⬅️ 2단계: 좌측 이동 (유령 체이스 - 도망)
    
    // 👈 선두에서 도망치는 팩맨 (좌측을 바라보며 입이 쩍 벌어진 상태 고정)
    u8g2.drawDisc(x, baseY, 3);
    
    // 입 모양 탈루 (왼쪽으로 향하도록 좌표 계산)
    u8g2.setDrawColor(0);
    u8g2.drawTriangle(x, baseY, x - 4, baseY - 2, x - 4, baseY + 2); 
    u8g2.setDrawColor(1);

    // 16픽셀 뒤에서 따라오는 유령
    int ghostX = x + 16; 
    
    u8g2.drawBox(ghostX - 2, baseY - 1, 5, 4);      
    u8g2.drawPixel(ghostX - 1, baseY - 2);          
    u8g2.drawPixel(ghostX, baseY - 2);
    u8g2.drawPixel(ghostX + 1, baseY - 2);
    u8g2.drawPixel(ghostX - 3, baseY);              
    u8g2.drawPixel(ghostX + 3, baseY);
    
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

// -------------------------------------------------------------
// 아장아장 고양이 애니메이션 함수
// -------------------------------------------------------------
void drawCat(int x, int dir) {
  int baseY = 33;
  
  if (dir == 1) {
    u8g2.drawBox(x - 3, baseY - 1, 5, 3); 
    u8g2.drawBox(x + 2, baseY - 3, 3, 3); 
    u8g2.drawPixel(x + 2, baseY - 4);
    u8g2.drawPixel(x + 4, baseY - 4);
    u8g2.drawVLine(x - 4, baseY - 3, 3); 

    if (animFrame % 2 == 0) {
      u8g2.drawPixel(x - 2, baseY + 2);
      u8g2.drawPixel(x + 1, baseY + 2);
    } else {
      u8g2.drawPixel(x - 3, baseY + 2);
      u8g2.drawPixel(x + 2, baseY + 2);
    }
  } else {
    u8g2.drawBox(x - 2, baseY - 1, 5, 3); 
    u8g2.drawBox(x - 5, baseY - 3, 3, 3); 
    u8g2.drawPixel(x - 5, baseY - 4);
    u8g2.drawPixel(x - 3, baseY - 4);
    u8g2.drawVLine(x + 3, baseY - 3, 3); 

    if (animFrame % 2 == 0) {
      u8g2.drawPixel(x - 3, baseY + 2);
      u8g2.drawPixel(x + 1, baseY + 2);
    } else {
      u8g2.drawPixel(x - 4, baseY + 2);
      u8g2.drawPixel(x, baseY + 2);
    }
  }
}

// -------------------------------------------------------------
// 통통 튀는 하트 애니메이션 함수
// -------------------------------------------------------------
void drawBouncingHeart(int x) {
  int bounceY = 32 + (int)(sin(x * 0.2) * 3);
  u8g2.drawPixel(x - 1, bounceY - 2);
  u8g2.drawPixel(x + 1, bounceY - 2);
  u8g2.drawBox(x - 2, bounceY - 1, 5, 2);
  u8g2.drawBox(x - 1, bounceY + 1, 3, 1);
  u8g2.drawPixel(x, bounceY + 2);
}

// -------------------------------------------------------------
// 애니메이션 핑퐁 제어 및 모드 분기 핸들러
// -------------------------------------------------------------
void handleAnimation() {
  iconX += iconDirection;
  animFrame++;

  // 두 마리가 같이 움직이는 것을 고려하여 최대 바운더리 마진 최적화
  int maxRight = (ANIMATION_MODE == 1) ? 100 : 114; 
  int minLeft = 10;

  if (iconX >= maxRight) {
    iconX = maxRight;
    iconDirection = -1;
  } else if (iconX <= minLeft) {
    iconX = minLeft;
    iconDirection = 1;
  }

  switch (ANIMATION_MODE) {
    case 1: drawPacman(iconX, iconDirection); break;
    case 2: drawCat(iconX, iconDirection); break;
    case 3: drawBouncingHeart(iconX); break;
    default: u8g2.drawHLine(iconX - 4, 33, 8); break;
  }
}

// -------------------------------------------------------------
// 디스플레이 통합 출력 함수
// -------------------------------------------------------------
void displayAllInfo(struct tm timeinfo) {
  u8g2.clearBuffer();

  // [1단] 날짜와 시간 표시 (7x14B Bold 적용)
  char dateTimeBuffer[30];
  sprintf(dateTimeBuffer, "%02d-%02d  %02d:%02d:%02d", 
          timeinfo.tm_mon + 1, timeinfo.tm_mday, 
          timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
  
  u8g2.setFont(u8g2_font_7x14B_mn);  

  int dateTimeWidth = u8g2.getStrWidth(dateTimeBuffer);
  u8g2.drawStr((128 - dateTimeWidth) / 2, 20, dateTimeBuffer);

  // [중앙단] 수정된 애니메이션 탑재
  handleAnimation();

  // [2단] 현재 지역 표시 (Y=44)
  u8g2.setFont(u8g2_font_6x10_tf); 
  int cityStrWidth = u8g2.getStrWidth(currentCity.c_str());
  u8g2.drawStr((128 - cityStrWidth) / 2, 44, currentCity.c_str());

  // [3단] 최하단 날씨, 온도, 습도 표시 (Y=58)
  char weatherBuffer[30];
  sprintf(weatherBuffer, "%s  %.1f\xb0" "C  %d%%", weatherMain.c_str(), temperature, humidity);
  u8g2.setFont(u8g2_font_6x10_tf); 
  int weatherStrWidth = u8g2.getStrWidth(weatherBuffer);
  u8g2.drawStr((128 - weatherStrWidth) / 2, 58, weatherBuffer);

  u8g2.sendBuffer();
}

void setup() {
  Serial.begin(115200);
  u8g2.begin();  
  
  connectWiFi();        
  getCurrentCity();     
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer); 
  getWeatherData();     
}

void loop() {
  unsigned long currentMillis = millis();
  
  if (currentMillis - lastWeatherCheck >= weatherInterval) {
    lastWeatherCheck = currentMillis;
    getWeatherData();
  }

  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    return;
  }

  if (currentMillis - lastIconUpdate >= iconInterval) {
    lastIconUpdate = currentMillis;
    displayAllInfo(timeinfo); 
  }
}