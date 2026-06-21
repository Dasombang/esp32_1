#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <U8g2lib.h>
#include <SPI.h>
#include "time.h"
#include "secrets.h"

/*
ESP32용 WiFi 라이브러리(WiFi.h) 내부에서 미리 정의된 상태 값
typedef enum {
    WL_NO_SHIELD        = 255,   // WiFi 실드가 없음
    WL_IDLE_STATUS      = 0,     // 유휴 상태
    WL_NO_SSID_AVAIL    = 1,     // SSID를 찾을 수 없음
    WL_SCAN_COMPLETED   = 2,     // 스캔 완료
    WL_CONNECTED        = 3,     // 연결 성공
    WL_CONNECT_FAILED   = 4,     // 연결 실패
    WL_CONNECTION_LOST  = 5,     // 연결 끊김
    WL_DISCONNECTED     = 6      // 연결 해제됨
} wl_status_t;


*/

// secrets.h 설정 값
const char* ssid     = SECRET_SSID;
const char* password = SECRET_PASSWORD;
const char* apiKey   = SECRET_API_KEY;

// 대한민국 시간 설정 (UTC+9)
const long  gmtOffset_sec = 32400;
const int   daylightOffset_sec = 0; 
const char* ntpServer = "pool.ntp.org";

// 폰트 정의
const uint8_t* TIME_FONT = u8g2_font_7x14B_mn;
const uint8_t* LOGO_FONT = u8g2_font_9x18B_tf;//u8g2_font_courB10_tf
const uint8_t* MSG_FONT = u8g2_font_5x8_tf;
//const uint8_t* ICON_FONT = u8g2_font_open_iconic_embedded_1x_t;


// ST7567A 하드웨어 SPI 디스플레이 설정
U8G2_ST7567_OS12864_F_4W_HW_SPI u8g2(U8G2_R0, /* cs=*/ 5, /* dc=*/ 21, /* reset=*/ 22);

// [애니메이션 모드 선택] 1: 팩맨 | 2: 고양이 | 3: 하트 | 4: 물고기 | 5: 공 | 6: 로봇 | 7: 눈(Eyes)
const int ANIMATION_MODE = 1; 

// 전역 변수
unsigned long lastWeatherCheck = 0;
const unsigned long weatherInterval = 600000; 

// 타임아웃 및 점 개수 설정 변수
const unsigned long TIMEOUT_MS = 10000; //30000
const int MAX_DOTS = 3;

String currentCity = "Seoul";   
String weatherMain = "---";     
float temperature = 0.0;        
int humidity = 0;               

// 통신 에러 플래그 (도시나 날씨 실패 시 true)
bool hasCommunicationError = false;

int iconX = 6;           
int iconDirection = 1;    
unsigned long lastIconUpdate = 0;
const unsigned long iconInterval = 50; 
int animFrame = 0;        

// [상태 관리 플래그] 통신 안내 문구가 출력 중일 때는 메인 화면 리프레시를 일시 차단하여 깜빡임 원천 제거
bool isConnecting = false;

// 하위 함수 전방 선언
void connectWiFi();
void getCurrentCity();
void syncTime();
void getWeatherData();
void handleAnimation();

// -------------------------------------------------------------
// UI 전용 함수
// -------------------------------------------------------------
void drawIntroLogo() {
  const char* introLogo = "DASOMBANG";
  u8g2.setFont(LOGO_FONT);
  u8g2.drawStr((128 - u8g2.getStrWidth(introLogo)) / 2, 28, introLogo);
}

void updateStatus(const char* msg, int frameStep) {
  u8g2.clearBuffer();
  drawIntroLogo(); 
  
  u8g2.setFont(MSG_FONT); 
  int msgWidth = u8g2.getStrWidth(msg);
  
  int msgX = (frameStep == 0) ? (128 - msgWidth) / 2 : (128 - (msgWidth + 24)) / 2;
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
  isConnecting = true; // 안내 문구 모드 활성화
  const char* statusMsg = "Connecting to WiFi";
  const char* failMsg   = "WiFi Connect Failed";

  WiFi.begin(ssid, password);
  
  int dotCount = 0;
  unsigned long lastUpdate = 0;
  unsigned long startAttemptTime = millis(); 
  
  u8g2.setFont(MSG_FONT);
  int msgWidth = u8g2.getStrWidth(statusMsg);
  int totalWidth = msgWidth + 2 + (MAX_DOTS * 4);
  int msgX = (128 - totalWidth) / 2;
  
  while (WiFi.status() != WL_CONNECTED) {
    if (millis() - startAttemptTime >= TIMEOUT_MS) {
      updateStatus(failMsg, 0);
      Serial.println("WiFi Connection Timeout! Program Halted.");
      while (true) { delay(1000); } 
    }

    if (millis() - lastUpdate >= 300) {
      lastUpdate = millis();
      u8g2.clearBuffer();
      drawIntroLogo();
      u8g2.setFont(MSG_FONT);
      u8g2.drawStr(msgX, 52, statusMsg);
      for (int i = 0; i < dotCount; i++) {
        u8g2.drawStr(msgX + msgWidth + 2 + (i * 4), 52, ".");
      }
      u8g2.sendBuffer();
      dotCount = (dotCount + 1) % (MAX_DOTS + 1); 
    }
    delay(10);
  }

  Serial.println(""); // 줄바꿈
  Serial.print("WiFi Connected! ");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP()); // IPAddress 객체를 직접 출력
  isConnecting = false; // 안내 문구 모드 해제
}

void getCurrentCity() {
  if (WiFi.status() == WL_CONNECTED) {
    isConnecting = true; // 안내 문구 모드 활성화
    const char* statusMsg = "Finding City";

    HTTPClient http;
    http.begin("http://ip-api.com/json/?lang=en"); 
    
    int dotCount = 0;
    unsigned long lastUpdate = 0;
    
    u8g2.setFont(MSG_FONT);
    int msgWidth = u8g2.getStrWidth(statusMsg);
    int totalWidth = msgWidth + 2 + (MAX_DOTS * 4);
    int msgX = (128 - totalWidth) / 2;

    unsigned long startActionTime = millis();
    while (http.connected() == false && (millis() - startActionTime < 2000)) {
      if (millis() - lastUpdate >= 300) {
        lastUpdate = millis();
        u8g2.clearBuffer();
        drawIntroLogo();
        u8g2.setFont(MSG_FONT);
        u8g2.drawStr(msgX, 52, statusMsg);
        for (int i = 0; i < dotCount; i++) {
          u8g2.drawStr(msgX + msgWidth + 2 + (i * 4), 52, ".");
        }
        u8g2.sendBuffer();
        dotCount = (dotCount + 1) % (MAX_DOTS + 1);
      }
      delay(10);
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
          // hasCommunicationError = false; 
          Serial.println("Successfully found the city. The name of the city is " + currentCity + ".");
          http.end();
          isConnecting = false; // 정상 종료 시 해제
          return;
        }
      }
    }
    http.end();
    //currentCity = "Seoul"; 
    hasCommunicationError = true;
    Serial.println("Failed to find the city! Set to Seoul.");
    isConnecting = false; // 종료 시 해제
  }
}

void syncTime() {
  if (WiFi.status() == WL_CONNECTED) {
    isConnecting = true; // 안내 문구 모드 활성화
    const char* statusMsg = "Syncing Time";
    const char* failMsg   = "Time Sync Failed";

    int dotCount = 0;
    unsigned long lastUpdate = 0;
    unsigned long startAttemptTime = millis();
    
    u8g2.setFont(MSG_FONT);
    int msgWidth = u8g2.getStrWidth(statusMsg);
    int totalWidth = msgWidth + 2 + (MAX_DOTS * 4);
    int msgX = (128 - totalWidth) / 2;

    // 1. 고정 문구 즉시 출력 (기존 방식 유지)
    u8g2.clearBuffer();
    drawIntroLogo();
    u8g2.setFont(MSG_FONT);
    u8g2.drawStr(msgX, 52, statusMsg);
    u8g2.sendBuffer();
    
    // 2. 점 3개 노출 애니메이션 (기존 configTime 전 1회 실행)
    for (int i = 1; i <= MAX_DOTS; i++) {
      delay(300);
      u8g2.drawStr(msgX + msgWidth + 2 + ((i - 1) * 4), 52, ".");
      u8g2.sendBuffer();
    }

    // 3. NTP 시간 동기화 시작 (요청하신 위치)
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);

    struct tm timeinfo;
    
    // 4. 시간 동기화 성공 시까지 애니메이션 루프 (기존 로직 유지)
    while (!getLocalTime(&timeinfo)) {
      // 30초 타임아웃 발생 시 실패 메시지 출력 후 재시작 (기존 로직 유지)
      if (millis() - startAttemptTime >= TIMEOUT_MS) {
        updateStatus(failMsg, 0);
        Serial.println("NTP Time Sync Timeout! Restarting...");
        delay(2000);
        ESP.restart();
      }

      // 5. 300ms 주기로 점 애니메이션 갱신 (기존 로직 유지)
      if (millis() - lastUpdate >= 300) {
        lastUpdate = millis();
        u8g2.clearBuffer();
        drawIntroLogo();
        u8g2.setFont(MSG_FONT);
        u8g2.drawStr(msgX, 52, statusMsg);
        
        for (int i = 0; i < dotCount; i++) {
          u8g2.drawStr(msgX + msgWidth + 2 + (i * 4), 52, ".");
        }
        u8g2.sendBuffer();
        dotCount = (dotCount + 1) % (MAX_DOTS + 1);
      }
      delay(10);
    }
    Serial.println("Time synchronization successful.");
    isConnecting = false; // 안내 문구 모드 해제
  }
}

void getWeatherData() {
  if (WiFi.status() == WL_CONNECTED) {
    isConnecting = true; // 안내 문구 모드 활성화 ("Loading Weather" 즉시 출력 및 대기 루프 작동 보장)
    const char* statusMsg = "Loading Weather";

    HTTPClient http;
    String url = "http://api.openweathermap.org/data/2.5/weather?q=" + currentCity + "&appid=" + String(apiKey) + "&units=metric";
    http.begin(url);
    
    int dotCount = 0;
    unsigned long lastUpdate = 0;
    
    u8g2.setFont(MSG_FONT);
    int msgWidth = u8g2.getStrWidth(statusMsg);
    int totalWidth = msgWidth + 2 + (MAX_DOTS * 4);
    int msgX = (128 - totalWidth) / 2;

    unsigned long startActionTime = millis();
    while (http.connected() == false && (millis() - startActionTime < 2000)) {
      if (millis() - lastUpdate >= 300) {
        lastUpdate = millis();
        u8g2.clearBuffer();
        drawIntroLogo();
        u8g2.setFont(MSG_FONT);
        u8g2.drawStr(msgX, 52, statusMsg);
        for (int i = 0; i < dotCount; i++) {
          u8g2.drawStr(msgX + msgWidth + 2 + (i * 4), 52, ".");
        }
        u8g2.sendBuffer();
        dotCount = (dotCount + 1) % (MAX_DOTS + 1);
      }
      delay(10);
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
        // hasCommunicationError = false;
        http.end();
        Serial.println("Weather information loading successful.");
        isConnecting = false; // 정상 종료 시 해제
        return; 
      }
    }
    http.end();
    weatherMain = "---";
    hasCommunicationError = true;
    Serial.println("Failed to get weather information.");
    isConnecting = false; // 에러 종료 시 해제
  }
}

// -------------------------------------------------------------
// 애니메이션 함수셋 (drawPacman, drawCat, drawBouncingHeart, handleAnimation 동일 유지)
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

void drawFish(int x, int dir) {
  int baseY = 30;
  int floatY = baseY + (int)(sin(animFrame * 0.2) * 4); // 32를 중심으로 유영
  
  if (dir == 1) {
    u8g2.drawDisc(x, floatY, 3);
    u8g2.drawTriangle(x-4, floatY, x-6, floatY-3, x-6, floatY+3);
  } else {
    u8g2.drawDisc(x, floatY, 3);
    u8g2.drawTriangle(x+4, floatY, x+6, floatY-3, x+6, floatY+3);
  }
}

void drawRobot(int x) {
  int baseY = 32; // 기준 높이
  int bodyH = 6;  // 몸통 높이
  
  // 몸통: baseY - 6에서 시작하여 높이 6만큼 그림
  // 몸통의 하단 Y 좌표는 (baseY - 6) + 6 = baseY가 됨
  u8g2.drawBox(x - 4, baseY - 6, 8, bodyH); 
  
  // 다리 시작 위치를 몸통 하단(baseY)에 딱 붙임
  int legY = baseY; 
  
  if (animFrame % 4 < 2) {
    u8g2.drawVLine(x - 2, legY, 4); // 왼쪽 다리 굽힘
    u8g2.drawVLine(x + 2, legY, 6); // 오른쪽 다리 펴짐
  } else {
    u8g2.drawVLine(x - 2, legY, 6); // 왼쪽 다리 펴짐
    u8g2.drawVLine(x + 2, legY, 4); // 오른쪽 다리 굽힘
  }
  
  // 눈 위치 (몸통 내부 중앙쯤으로 조정)
  u8g2.drawPixel(x - 2, baseY - 4); 
  u8g2.drawPixel(x + 2, baseY - 4); 
}

void drawRollingBall(int x) {
  int baseY = 30;
  int bounceY = baseY - abs((int)(sin(x * 0.15) * 7)); // 튀어 오르기
  
  u8g2.drawCircle(x, bounceY, 4); // 공 외곽
  
  // 회전하는 내부 + 표시 (animFrame으로 회전각 시뮬레이션)
  if (animFrame % 4 < 2) {
    u8g2.drawVLine(x, bounceY - 3, 7);
    u8g2.drawHLine(x - 3, bounceY, 7);
  } else {
    u8g2.drawLine(x - 3, bounceY - 3, x + 3, bounceY + 3);
    u8g2.drawLine(x + 3, bounceY - 3, x - 3, bounceY + 3);
  }
}

void drawEyes(int x) { // x는 좌우 이동을 위한 중심값
  int baseY = 32;
  int eyeDist = 8; // 두 눈 사이의 거리
  
  // 1. 눈 깜빡임 로직: 50프레임 중 5프레임 동안 눈을 감음
  bool isBlinking = (animFrame % 50 < 5);
  
  // 2. 눈동자 랜덤 움직임 로직 (매 30프레임마다 변경)
  static int pupilOffset = 0;
  if (animFrame % 30 == 0) {
    pupilOffset = random(-1, 2); // -1(좌), 0(중앙), 1(우)
  }

  // 눈 외곽 그리기 (흰자)
  u8g2.drawCircle(x - eyeDist, baseY, 4);
  u8g2.drawCircle(x + eyeDist, baseY, 4);

  if (isBlinking) {
    // 눈을 감았을 때 (가로선)
    u8g2.drawHLine(x - eyeDist - 2, baseY, 5);
    u8g2.drawHLine(x + eyeDist - 2, baseY, 5);
  } else {
    // 눈을 떴을 때 (눈동자 그리기)
    u8g2.drawPixel(x - eyeDist + pupilOffset, baseY);
    u8g2.drawPixel(x + eyeDist + pupilOffset, baseY);
  }
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
    case 4: drawFish(iconX, iconDirection); break; // 추가
    case 5: drawRollingBall(iconX); break;         // 추가
    case 6: drawRobot(iconX); break;               // 추가
    case 7: drawEyes(64); break; // 64는 화면 가로 중앙값
    default: u8g2.drawHLine(iconX - 4, 32, 8); break; 
  }
}

// -------------------------------------------------------------
// 메인 디스플레이 출력 함수
// -------------------------------------------------------------
void displayAllInfo(struct tm timeinfo) {
  u8g2.clearBuffer(); 
  
  static char dateTimeBuffer[30]; 
  sprintf(dateTimeBuffer, "%02d-%02d  %02d:%02d:%02d", 
          timeinfo.tm_mon + 1, timeinfo.tm_mday, 
          timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
  u8g2.setFont(TIME_FONT);  
  u8g2.drawStr((128 - u8g2.getStrWidth(dateTimeBuffer)) / 2, 20, dateTimeBuffer);

  handleAnimation();

  u8g2.setFont(u8g2_font_6x10_tf); 
  u8g2.drawStr((128 - u8g2.getStrWidth(currentCity.c_str())) / 2, 44, currentCity.c_str()); 

  if (weatherMain != "---") {
    char weatherBuffer[35]; 
    sprintf(weatherBuffer, "%s  %.1f\xb0" "C  %d%%", weatherMain.c_str(), temperature, humidity); 
    u8g2.setFont(u8g2_font_6x10_tf); 
    u8g2.drawStr((128 - u8g2.getStrWidth(weatherBuffer)) / 2, 58, weatherBuffer); 
  }

  // Serial.println("hasCommunicationError = " + String(hasCommunicationError));
  if (hasCommunicationError) {
    u8g2.setFont(u8g2_font_open_iconic_embedded_1x_t);
    u8g2.drawGlyph(116, 10, 0x0042); 
    Serial.println("Icon drawGlyph called at 116, 10"); // 실행 여부 확인
  }

  u8g2.sendBuffer();
}

// -------------------------------------------------------------
// 메인 루프 진입부
// -------------------------------------------------------------
void setup() {
  Serial.begin(115200);
  u8g2.begin();
  // SPI 클럭을 낮추어 데이터 전송 안정성을 높임 (지글거림 방지)
  u8g2.setBusClock(1000000);    
  connectWiFi();        
  getCurrentCity();     
  getWeatherData();     
  syncTime();
}

void loop() {
  unsigned long currentMillis = millis(); 
  
  // 1. 날씨 데이터 갱신 (10분 주기, 기존과 동일)
  if (currentMillis - lastWeatherCheck >= weatherInterval) {
    lastWeatherCheck = currentMillis; 
    getWeatherData();
  } 
  
  // 2. [수정됨] 화면 갱신 및 애니메이션 (50ms 주기)
  if (currentMillis - lastIconUpdate >= iconInterval) {
    lastIconUpdate = currentMillis;
    
    if (!isConnecting) {
      // 시간 데이터는 1초마다 한 번씩만 캐싱해두어 연산 부하 제거
      static struct tm timeinfo;
      static unsigned long lastTimeSync = 0;
      if (currentMillis - lastTimeSync >= 1000) {
        // 0ms 대기 대신 1ms 타임아웃을 주어 시스템 정지 방지
        getLocalTime(&timeinfo, 1);
        lastTimeSync = currentMillis;
      }
      
      // [근본 해결]
      // 기존에는 이 안에서 연산과 통신이 혼재되어 지글거림이 발생했습니다.
      // 이제 여기서 displayAllInfo()를 호출할 때, 
      // 이 함수가 이전 루프보다 더 빨리 끝나더라도 통신은 50ms마다 딱 한 번만 수행됩니다.
      displayAllInfo(timeinfo); 
    }
  }
}