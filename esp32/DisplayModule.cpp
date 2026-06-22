#include "DisplayModule.h"
#include "NetworkModule.h"

// 하드웨어 SPI 인스턴스 정의
U8G2_ST7567_OS12864_F_4W_HW_SPI u8g2(U8G2_R0, /* cs=*/ 5, /* dc=*/ 21, /* reset=*/ 22);

// 폰트 매핑
const uint8_t* TIME_FONT = u8g2_font_7x14B_mn;
const uint8_t* LOGO_FONT = u8g2_font_9x18B_tf;
const uint8_t* MSG_FONT = u8g2_font_5x8_tf;

const int ANIMATION_MODE = 1; 
int iconX = 6;           
int iconDirection = 1;    
int animFrame = 0;        

void initDisplay() {
  SPI.begin(18, 19, 23, 5); // HW SPI 핀 수동 동기화 고정
  u8g2.begin();
  u8g2.setBusClock(8000000); 
}

void drawIntroLogo() {
  const char* introLogo = "DASOMBANG";
  u8g2.setFont(LOGO_FONT);
  u8g2.drawStr((128 - u8g2.getStrWidth(introLogo)) / 2, 28, introLogo);
}

void updateStatus(const char* msg, int frameStep) {
  u8g2.clearBuffer();
  drawIntroLogo(); 
  
  u8g2.setFont(MSG_FONT); 
  
  // 1. 순수 안내 문구(msg)만의 길이를 계산하여 시작 X 좌표를 딱 고정합니다.
  int msgWidth = u8g2.getStrWidth(msg);
  
  // 팁: 최대 점 3개(약 12픽셀)가 추가될 것을 감안하여 
  // 전체 화면 중앙에서 약간 왼쪽으로 기본 고정 위치를 잡습니다.
  int msgX = (128 - msgWidth) / 2 - 4; 
  if (msgX < 0) msgX = 0; // 화면 왼쪽 이탈 방지
  
  // 2. 고정된 위치에 문구 출력
  u8g2.drawStr(msgX, 52, msg);
  
  // 3. 점(...)은 문구가 끝난 지점(msgX + msgWidth + 2)부터 고정된 간격으로 추가 생성
  for (int i = 0; i < frameStep; i++) {
    u8g2.drawStr(msgX + msgWidth + 2 + (i * 4), 52, ".");
  }
  
  u8g2.updateDisplay();
}

// 애니메이션 렌더링 서브셋
void drawPacman(int x, int dir) {
  int baseY = 32;
  if (dir == 1) {
    for (int dotX = 12; dotX < 120; dotX += 12) {
      if (dotX > x + 3) u8g2.drawPixel(dotX, baseY);
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
      u8g2.drawPixel(ghostX - 2, baseY + 3); u8g2.drawPixel(ghostX, baseY + 3); u8g2.drawPixel(ghostX + 2, baseY + 3);
    } else {
      u8g2.drawPixel(ghostX - 1, baseY + 3); u8g2.drawPixel(ghostX + 1, baseY + 3);
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
  int baseY = 30; int floatY = baseY + (int)(sin(animFrame * 0.2) * 4);
  if (dir == 1) { u8g2.drawDisc(x, floatY, 3); u8g2.drawTriangle(x-4, floatY, x-6, floatY-3, x-6, floatY+3); } 
  else { u8g2.drawDisc(x, floatY, 3); u8g2.drawTriangle(x+4, floatY, x+6, floatY-3, x+6, floatY+3); }
}

void drawRobot(int x) {
  int baseY = 32; int bodyH = 6;  
  u8g2.drawBox(x - 4, baseY - 6, 8, bodyH); 
  int legY = baseY; 
  if (animFrame % 4 < 2) { u8g2.drawVLine(x - 2, legY, 4); u8g2.drawVLine(x + 2, legY, 6); } 
  else { u8g2.drawVLine(x - 2, legY, 6); u8g2.drawVLine(x + 2, legY, 4); }
  u8g2.drawPixel(x - 2, baseY - 4); u8g2.drawPixel(x + 2, baseY - 4); 
}

void drawRollingBall(int x) {
  int baseY = 30; int bounceY = baseY - abs((int)(sin(x * 0.15) * 7));
  u8g2.drawCircle(x, bounceY, 4); 
  if (animFrame % 4 < 2) { u8g2.drawVLine(x, bounceY - 3, 7); u8g2.drawHLine(x - 3, bounceY, 7); } 
  else { u8g2.drawLine(x - 3, bounceY - 3, x + 3, bounceY + 3); u8g2.drawLine(x + 3, bounceY - 3, x - 3, bounceY + 3); }
}

void drawEyes(int x) {
  int baseY = 32; int eyeDist = 8;
  bool isBlinking = (animFrame % 50 < 5);
  static int pupilOffset = 0;
  if (animFrame % 30 == 0) { pupilOffset = random(-1, 2); }
  u8g2.drawCircle(x - eyeDist, baseY, 4); u8g2.drawCircle(x + eyeDist, baseY, 4);
  if (isBlinking) { u8g2.drawHLine(x - eyeDist - 2, baseY, 5); u8g2.drawHLine(x + eyeDist - 2, baseY, 5); } 
  else { u8g2.drawPixel(x - eyeDist + pupilOffset, baseY); u8g2.drawPixel(x + eyeDist + pupilOffset, baseY); }
}

void handleAnimation() {
  iconX += iconDirection; animFrame++; 
  int maxRight = (ANIMATION_MODE == 1) ? 100 : 114; int minLeft = 10; 
  if (iconX >= maxRight) { iconX = maxRight; iconDirection = -1; } 
  else if (iconX <= minLeft) { iconX = minLeft; iconDirection = 1; } 

  u8g2.setDrawColor(1);
  switch (ANIMATION_MODE) { 
    case 1: drawPacman(iconX, iconDirection); break; 
    case 2: drawCat(iconX, iconDirection); break; 
    case 3: drawBouncingHeart(iconX); break; 
    case 4: drawFish(iconX, iconDirection); break;
    case 5: drawRollingBall(iconX); break;
    case 6: drawRobot(iconX); break;
    case 7: drawEyes(64); break;
    default: u8g2.drawHLine(iconX - 4, 32, 8); break; 
  }
}

void displayAllInfo(struct tm timeinfo) {
  static char dateTimeBuffer[20];
  static char weatherBuffer[35];
  static int lastSecond = -1;

  if (timeinfo.tm_sec != lastSecond) {
    sprintf(dateTimeBuffer, "%02d-%02d  %02d:%02d:%02d", 
            timeinfo.tm_mon + 1, timeinfo.tm_mday, 
            timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
    lastSecond = timeinfo.tm_sec;
  }

  // 데이터 교환 안정성 버그 개선 (원자적 치환 연산)
  if (weatherDataChanged.exchange(false)) {
    sprintf(weatherBuffer, "%s  %.1f\xb0" "C  %d%%", weatherMain.c_str(), temperature, humidity);
  }

  u8g2.clearBuffer(); 
  
  u8g2.setFont(TIME_FONT);  
  u8g2.drawStr((128 - u8g2.getStrWidth(dateTimeBuffer)) / 2, 20, dateTimeBuffer);

  handleAnimation();

  u8g2.setFont(u8g2_font_6x10_tf); 
  u8g2.drawStr((128 - u8g2.getStrWidth(currentCity.c_str())) / 2, 44, currentCity.c_str()); 

  if (weatherMain != "---") {
    u8g2.drawStr((128 - u8g2.getStrWidth(weatherBuffer)) / 2, 58, weatherBuffer); 
  }

  if (hasCommunicationError) {
    u8g2.setFont(u8g2_font_open_iconic_embedded_1x_t);
    u8g2.drawGlyph(116, 10, 0x0042); 
  }

  u8g2.sendBuffer();
}
