#ifndef LED_STATUS_H
#define LED_STATUS_H

#include "config.h"

enum LEDColor : uint8_t { LED_OFF, LED_RED, LED_GREEN, LED_BLUE, LED_YELLOW, LED_PURPLE, LED_CYAN, LED_WHITE };

#if defined(CONFIG_IDF_TARGET_ESP32S3)
// --- ESP32-S3: WS2812 NeoPixel na GPIO48 ---
#include <Adafruit_NeoPixel.h>

static Adafruit_NeoPixel _statusPixel(NEOPIXEL_COUNT, NEOPIXEL_PIN, NEO_GRB + NEO_KHZ800);

inline void ledBegin() {
  _statusPixel.begin();
  _statusPixel.setBrightness(30);  // Nizka svetilnost za statusni indikator
  _statusPixel.show();
}

inline void ledSetRGB(uint8_t r, uint8_t g, uint8_t b) {
  _statusPixel.setPixelColor(0, _statusPixel.Color(r, g, b));
  _statusPixel.show();
}

#else
// --- ESP32: 3× LEDC PWM (common-anode, invertirana logika) ---
inline void ledBegin() {
  ledcAttach(LED_R_PIN, 5000, 8);
  ledcAttach(LED_G_PIN, 5000, 8);
  ledcAttach(LED_B_PIN, 5000, 8);
}

inline void ledSetRGB(uint8_t r, uint8_t g, uint8_t b) {
  ledcWrite(LED_R_PIN, 255 - r);
  ledcWrite(LED_G_PIN, 255 - g);
  ledcWrite(LED_B_PIN, 255 - b);
}
#endif

// Skupna barvna tabela (obe platformi)
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
