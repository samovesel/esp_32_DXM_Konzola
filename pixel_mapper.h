#ifndef PIXEL_MAPPER_H
#define PIXEL_MAPPER_H

#include "config.h"

#if defined(CONFIG_IDF_TARGET_ESP32S3)

#include "fixture_engine.h"
#include "sound_engine.h"

class PixelMapper {
public:
  void begin();
  void update(const uint8_t* dmxOut, FixtureEngine* fixtures,
              SoundEngine* sound, float dt);

  PixelMapConfig& getConfig() { return _cfg; }
  void setConfig(const PixelMapConfig& cfg);
  bool isActive() const { return _cfg.enabled && _strip != nullptr; }

  void saveConfig();
  void loadConfig();

private:
  PixelMapConfig _cfg;
  void* _strip = nullptr;   // Adafruit_NeoPixel* (forward declared to avoid header dep)

  // Internal mode handlers
  void modeFixture(const uint8_t* dmxOut, FixtureEngine* fixtures);
  void modeVuMeter(SoundEngine* sound);
  void modeSpectrum(SoundEngine* sound);
  void modePulse(SoundEngine* sound, float dt);

  float _pulsePhase = 0;
};

#endif // CONFIG_IDF_TARGET_ESP32S3
#endif
