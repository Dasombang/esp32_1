#include "NetworkModule.h"
#include "DisplayModule.h"
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "secrets.h"

// secrets.h 매핑
const char* ssid     = SECRET_SSID;
const char* password = SECRET_PASSWORD;
const char* apiKey   = SECRET_API_KEY;

// 타임 서버 설정
const long  gmtOffset_sec = 32400;
const int   daylightOffset_sec = 0; 
const char* ntpServer = "pool.ntp.org";
const unsigned long TIMEOUT_MS = 10000;
const int MAX_DOTS = 3;

// 전역 변수 정의
String currentCity = "Seoul";   
String weatherMain = "---";     
float temperature = 0.0;        
int humidity = 0;               
volatile bool hasCommunicationError = false;
unsigned long lastWeatherCheck = 0;

std::atomic<bool> isConnecting(false);
std::atomic<bool> weatherDataChanged(true);

void connectWiFi() {
  isConnecting = true; 
  const char* statusMsg = "Connecting to WiFi";
  const char* failMsg   = "WiFi Connect Failed";

  WiFi.begin(ssid, password);
  
  int dotCount = 0;
  unsigned long lastUpdate = 0;
  unsigned long startAttemptTime = millis(); 
  
  while (WiFi.status() != WL_CONNECTED) {
    if (millis() - startAttemptTime >= TIMEOUT_MS) {
      updateStatus(failMsg, 0);
      while (true) { delay(1000); } 
    }

    if (millis() - lastUpdate >= 300) {
      lastUpdate = millis();
      updateStatus(statusMsg, dotCount);
      dotCount = (dotCount + 1) % (MAX_DOTS + 1); 
    }
    vTaskDelay(pdMS_TO_TICKS(10));
  }
  isConnecting = false; 
}

void getCurrentCity() {
  if (WiFi.status() != WL_CONNECTED) return;
  
  isConnecting = true; 
  const char* statusMsg = "Finding City";

  HTTPClient http;
  http.begin("http://ip-api.com/json/?lang=en"); 
  http.setTimeout(3000);     
  http.setReuse(false);      
  
  int dotCount = 0;
  unsigned long lastUpdate = 0;
  unsigned long startActionTime = millis();

  while (http.connected() == false && (millis() - startActionTime < 2000)) {
    if (millis() - lastUpdate >= 300) {
      lastUpdate = millis();
      updateStatus(statusMsg, dotCount);
      dotCount = (dotCount + 1) % (MAX_DOTS + 1);
    }
    vTaskDelay(pdMS_TO_TICKS(10));
  }

  int httpCode = http.GET();
  if (httpCode == HTTP_CODE_OK) {
    String payload = http.getString();
    
    // 구조 최적화: JsonDocument 스택 영역 할당으로 메모리 파편화 방지
    JsonDocument localDoc;
    DeserializationError error = deserializeJson(localDoc, payload);
    if (!error) {
      String status = localDoc["status"].as<String>();
      if (status == "success") {
        currentCity = localDoc["city"].as<String>();
        http.end();
        isConnecting = false; 
        return;
      }
    }
  }
  http.end();
  hasCommunicationError = true;
  isConnecting = false; 
}

void syncTime() {
  if (WiFi.status() != WL_CONNECTED) return;

  isConnecting = true; 
  const char* statusMsg = "Syncing Time";
  const char* failMsg   = "Time Sync Failed";

  int dotCount = 0;
  unsigned long lastUpdate = 0;
  unsigned long startAttemptTime = millis();
  
  updateStatus(statusMsg, 0);
  for (int i = 1; i <= MAX_DOTS; i++) {
    delay(300);
    updateStatus(statusMsg, i);
  }

  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);

  time_t now;
  struct tm timeinfo;
  
  while (true) {
    time(&now);
    if (now > 1700000000) {   
        localtime_r(&now, &timeinfo);
        break;
    }

    if (millis() - startAttemptTime >= TIMEOUT_MS) {
        updateStatus(failMsg, 0);
        delay(2000);
        ESP.restart();
    }

    if (millis() - lastUpdate >= 300) {
        lastUpdate = millis();
        updateStatus(statusMsg, dotCount);
        dotCount = (dotCount + 1) % (MAX_DOTS + 1);
    }
    vTaskDelay(pdMS_TO_TICKS(10));
  }
  isConnecting = false; 
}

void getWeatherData() {
  if (WiFi.status() != WL_CONNECTED) return;

  isConnecting = true; 
  const char* statusMsg = "Loading Weather";

  HTTPClient http;
  char url[180];
  snprintf(url, sizeof(url), "http://api.openweathermap.org/data/2.5/weather?q=%s&appid=%s&units=metric", currentCity.c_str(), apiKey);
  http.begin(url);
  http.setTimeout(3000);     
  http.setReuse(false);      
  
  int dotCount = 0;
  unsigned long lastUpdate = 0;
  unsigned long startActionTime = millis();

  while (http.connected() == false && (millis() - startActionTime < 2000)) {
    if (millis() - lastUpdate >= 300) {
      lastUpdate = millis();
      updateStatus(statusMsg, dotCount);
      dotCount = (dotCount + 1) % (MAX_DOTS + 1);
    }
    vTaskDelay(pdMS_TO_TICKS(10));
  }

  int httpCode = http.GET();
  if (httpCode == HTTP_CODE_OK) { 
    String payload = http.getString();
    
    JsonDocument localDoc;
    DeserializationError error = deserializeJson(localDoc, payload);
    
    if (error == DeserializationError::Ok) {
      weatherMain = localDoc["weather"][0]["main"].as<String>();
      temperature = localDoc["main"]["temp"].as<float>();
      humidity = localDoc["main"]["humidity"].as<int>();

      weatherDataChanged = true;
      hasCommunicationError = false; 
      
      http.end();
      isConnecting = false; 
      return; 
    }
  }

  http.end();
  weatherMain = "---";
  temperature = 0.0;
  humidity = 0;
  hasCommunicationError = true; 
  weatherDataChanged = true;    
  isConnecting = false; 
}
