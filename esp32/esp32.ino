#include <Arduino.h>
#include "DisplayModule.h"
#include "NetworkModule.h"
#include "time.h"

// 공유 데이터 객체 및 동기화 락 제어용 뮤텍스
static struct tm sharedTimeinfo;
static SemaphoreHandle_t timeMutex;

// [Core 0 태스크]: 화면 렌더링 주기 전용 핸들러
void displayTask(void *pvParameters) {
  TickType_t lastWakeTime = xTaskGetTickCount();
  struct tm localCopy = {0}; // 뮤텍스 미확득 시 이전 시각 백업본 활용
  
  for (;;) {
    if (!isConnecting) {
      if (xSemaphoreTake(timeMutex, 0) == pdTRUE) {
        localCopy = sharedTimeinfo;
        xSemaphoreGive(timeMutex);
      }
      // 세마포어 경합 유무와 별개로 복사본을 활용하여 애니메이션이 끊기지 않게 렌더링 유지
      displayAllInfo(localCopy);
    }
    vTaskDelayUntil(&lastWakeTime, pdMS_TO_TICKS(85)); 
  }
}

void setup() {
  Serial.begin(115200);
  
  initDisplay(); // LCD 전원 및 드라이버 셋업
  timeMutex = xSemaphoreCreateMutex();

  // 순차 부팅 통신 가동
  connectWiFi();        
  getCurrentCity();     
  getWeatherData();     
  syncTime();

  // Core 0에 LCD 디스플레이 구동 전용 고순도 우선순위 태스크 스케줄링
  xTaskCreatePinnedToCore(
    displayTask,
    "DisplayTask",
    4096,
    NULL,
    1,
    NULL,
    0
  );
}

void loop() {
  unsigned long currentMillis = millis(); 
  
  // 10분 주기 날씨 리프레시 (Core 1)
  if (currentMillis - lastWeatherCheck >= weatherInterval) {
    lastWeatherCheck = currentMillis; 
    getWeatherData();
  } 
  
  // 1초 주기 시스템 시계 동기화 업링크 (Core 1)
  static unsigned long lastTimeSync = 0;
  if (currentMillis - lastTimeSync >= 1000) {
    time_t now;
    struct tm tempInfo;
    time(&now);
    localtime_r(&now, &tempInfo);
    
    if (xSemaphoreTake(timeMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
      sharedTimeinfo = tempInfo;
      xSemaphoreGive(timeMutex);
    }
    lastTimeSync = currentMillis;
  }
  
  vTaskDelay(pdMS_TO_TICKS(10)); // 시스템 아이들 타임 및 소프트웨어 워치독 오버플로우 방지
}