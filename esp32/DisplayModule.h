#ifndef DISPLAY_MODULE_H
#define DISPLAY_MODULE_H

#include <Arduino.h>
#include <U8g2lib.h>
#include <SPI.h>
#include "time.h"

extern U8G2_ST7567_OS12864_F_4W_HW_SPI u8g2;

// 폰트 정의 외부 참조
extern const uint8_t* TIME_FONT;
extern const uint8_t* LOGO_FONT;
extern const uint8_t* MSG_FONT;

// 함수 선언
void initDisplay();
void drawIntroLogo();
void updateStatus(const char* msg, int frameStep);
void displayAllInfo(struct tm timeinfo);
void handleAnimation();

#endif
