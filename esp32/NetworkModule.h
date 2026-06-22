#ifndef NETWORK_MODULE_H
#define NETWORK_MODULE_H

#include <Arduino.h>
#include <WiFi.h>
#include <atomic>

// 전역 공유 변수 선언 (extern)
extern String currentCity;
extern String weatherMain;
extern float temperature;
extern int humidity;
extern volatile bool hasCommunicationError;

extern std::atomic<bool> isConnecting;
extern std::atomic<bool> weatherDataChanged;

// 타이밍 제어 변수
extern unsigned long lastWeatherCheck;
const unsigned long weatherInterval = 600000; 

// 함수 선언
void connectWiFi();
void getCurrentCity();
void syncTime();
void getWeatherData();

#endif
