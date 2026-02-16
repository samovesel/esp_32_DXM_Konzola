#ifndef LED_STATUS_H
#define LED_STATUS_H

#include "config.h"

enum LEDColor : uint8_t { LED_OFF, LED_RED, LED_GREEN, LED_BLUE, LED_YELLOW, LED_PURPLE, LED_CYAN, LED_WHITE };

inline void ledBegin() {
  ledcAttach(LED_R_PIN, 5000, 8);
  ledcAttach(LED_G_PIN, 5000, 8);
  ledcAttach(LED_B_PIN, 5000, 8);
}

inline void ledSetRGB(uint8_t r, uint8_t g, uint8_t b) {
  // Invertirana logika za skupna-anoda LED (anoda na 3.3V, LOW = sveti)
  ledcWrite(LED_R_PIN, 255 - r);
  ledcWrite(LED_G_PIN, 255 - g);
  ledcWrite(LED_B_PIN, 255 - b);
}

inline void ledSet(LEDColor c) {
  switch (c) {
    case LED_OFF:    ledSetRGB(0,0,0);       break;
    case LED_RED:    ledSetRGB(255,0,0);     break;
    case LED_GREEN:  ledSetRGB(0,255,0);     break;
    case LED_BLUE:   ledSetRGB(0,0,255);     break;
    case LED_YELLOW: ledSetRGB(255,180,0);   break;
    case LED_PURPLE: ledSetRGB(180,0,255);   break;
    case LED_CYAN:   ledSetRGB(0,255,255);   break;
    case LED_WHITE:  ledSetRGB(255,255,255); break;
  }
}

#endif
