#include <Arduino.h>
#include <WiFi.h>
#include <U8g2lib.h>
#include <SPI.h>
#include "time.h"
#include "secrets.h"  // 👈 분리한 와이파이 설정 파일 불러오기

// secrets.h에 정의된 변수를 가져와 대입합니다.
const char* ssid     = SECRET_SSID;
const char* password = SECRET_PASSWORD;

// 대한민국 시간 설정 (UTC+9)
const long  gmtOffset_sec = 32400;
const int   daylightOffset_sec = 0; 
const char* ntpServer = "pool.ntp.org";

// ST7567A 하드웨어 SPI 디스플레이 설정
U8G2_ST7567_OS12864_F_4W_HW_SPI u8g2(U8G2_R0, /* cs=*/ 5, /* dc=*/ 21, /* reset=*/ 22);

int lastSecond = -1;

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

  u8g2.clearBuffer();
  u8g2.drawStr(0, 15, "WiFi Connected!");
  u8g2.drawStr(0, 35, "Syncing Time...");
  u8g2.sendBuffer();
}

// 디스플레이에 날짜와 시간(초 포함)을 그리는 함수
void displayDateTime(struct tm timeinfo) {
  u8g2.clearBuffer();

  // 1. 날짜 표시 (가운데 정렬)
  char dateBuffer[20];
  sprintf(dateBuffer, "%04d-%02d-%02d", timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday);
  u8g2.setFont(u8g2_font_7x14_tf); 
  int dateStrWidth = u8g2.getStrWidth(dateBuffer);
  int date_x_centered = (128 - dateStrWidth) / 2;
  u8g2.drawStr(date_x_centered, 20, dateBuffer);

  // 2. 시간 표시 (가운데 정렬)
  char timeBuffer[15];
  sprintf(timeBuffer, "%02d:%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
  u8g2.setFont(u8g2_font_fur17_tf);
  int timeStrWidth = u8g2.getStrWidth(timeBuffer);
  int x_centered = (128 - timeStrWidth) / 2;
  u8g2.drawStr(x_centered, 55, timeBuffer);

  u8g2.sendBuffer();
}

void setup() {
  Serial.begin(115200);
  u8g2.begin();  
  
  connectWiFi();
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
}

void loop() {
  struct tm timeinfo;
  if(!getLocalTime(&timeinfo)){
    Serial.println("Failed to obtain time");
    return;
  }

  if (timeinfo.tm_sec != lastSecond) {
    lastSecond = timeinfo.tm_sec;
    Serial.printf("Time Updated: %02d:%02d:%02d\n", timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
    displayDateTime(timeinfo);
  }
  delay(200); 
}