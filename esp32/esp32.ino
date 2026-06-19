#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <U8g2lib.h>
#include <SPI.h>
#include "time.h"
#include "secrets.h"

// 와이파이 및 날씨 설정 가져오기
const char* ssid     = SECRET_SSID;
const char* password = SECRET_PASSWORD;
const char* apiKey   = SECRET_API_KEY;
const char* city     = SECRET_CITY;

// 대한민국 시간 설정 (UTC+9)
const long  gmtOffset_sec = 32400;
const int   daylightOffset_sec = 0; 
const char* ntpServer = "pool.ntp.org";

// ST7567A 하드웨어 SPI 디스플레이 설정
U8G2_ST7567_OS12864_F_4W_HW_SPI u8g2(U8G2_R0, /* cs=*/ 5, /* dc=*/ 21, /* reset=*/ 22);

// 전역 변수 설정
int lastSecond = -1;
unsigned long lastWeatherCheck = 0;
const unsigned long weatherInterval = 600000; // 날씨 갱신 주기: 10분 (600,000ms)

String weatherMain = "---";  // 날씨 상태 (Clear, Clouds, Rain 등)
float temperature = 0.0;     // 현재 온도

// Wi-Fi 연결 함수
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

// 인터넷에서 날씨 정보를 가져오는 함수
void getWeatherData() {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    // API 요청 URL 생성 (섭씨 온도를 위해 &units=metric 추가)
    String url = "http://api.openweathermap.org/data/2.5/weather?q=" + String(city) + "&appid=" + String(apiKey) + "&units=metric";
    
    http.begin(url);
    int httpCode = http.GET();
    
    if (httpCode > 0) {
      String payload = http.getString();
      
      // JSON 파싱을 위한 메모리 할당 (OpenWeatherMap 응답 크기에 맞춤)
      JsonDocument doc;
      DeserializationError error = deserializeJson(doc, payload);
      
      if (!error) {
        weatherMain = doc["weather"][0]["main"].as<String>();
        temperature = doc["main"]["temp"].as<float>();
        Serial.printf("Weather Updated: %s, %.1f C\n", weatherMain.c_str(), temperature);
      } else {
        Serial.print("JSON Parsing failed: ");
        Serial.println(error.c_str());
      }
    } else {
      Serial.println("Error on HTTP request");
    }
    http.end();
  }
}

// 디스플레이 출력 함수 (날짜, 시간, 날씨 배치 조정)
void displayAllInfo(struct tm timeinfo) {
  u8g2.clearBuffer();

  // 1. 날짜 표시 (상단 가운데 정렬)
  char dateBuffer[20];
  sprintf(dateBuffer, "%04d-%02d-%02d", timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday);
  u8g2.setFont(u8g2_font_6x10_tf); // 시간과 날씨 공간 확보를 위해 살짝 작은 폰트 적용
  int dateStrWidth = u8g2.getStrWidth(dateBuffer);
  u8g2.drawStr((128 - dateStrWidth) / 2, 12, dateBuffer);

  // 2. 시간 표시 (중앙 정렬, 큼직하게)
  char timeBuffer[15];
  sprintf(timeBuffer, "%02d:%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
  u8g2.setFont(u8g2_font_fur14_tf); 
  int timeStrWidth = u8g2.getStrWidth(timeBuffer);
  u8g2.drawStr((128 - timeStrWidth) / 2, 36, timeBuffer);

  // 구분선 그리기
  u8g2.drawHLine(0, 42, 128);

  // 3. 날씨 정보 표시 (하단 영역 배치)
  u8g2.setFont(u8g2_font_6x10_tf);
  
  // 왼쪽: 날씨 상태 (예: Clear, Rain)
  u8g2.drawStr(5, 58, weatherMain.c_str());
  
  // 오른쪽: 온도 표시 (예: 24.5 C)
  char tempBuffer[10];
  sprintf(tempBuffer, "%.1f C", temperature);
  int tempStrWidth = u8g2.getStrWidth(tempBuffer);
  u8g2.drawStr(123 - tempStrWidth, 58, tempBuffer);

  u8g2.sendBuffer();
}

void setup() {
  Serial.begin(115200);
  u8g2.begin();  
  
  connectWiFi();
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  
  // 부팅 시 날씨를 즉시 한 번 가져옵니다.
  getWeatherData();
}

void loop() {
  unsigned long currentMillis = millis();
  
  // 10분마다 날씨 정보 데이터 동기화
  if (currentMillis - lastWeatherCheck >= weatherInterval) {
    lastWeatherCheck = currentMillis;
    getWeatherData();
  }

  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    return;
  }

  // 1초마다 시간 및 화면 업데이트
  if (timeinfo.tm_sec != lastSecond) {
    lastSecond = timeinfo.tm_sec;
    displayAllInfo(timeinfo);
  }
  
  delay(200); 
}