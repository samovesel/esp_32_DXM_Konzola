#include "pixel_mapper.h"

#if defined(CONFIG_IDF_TARGET_ESP32S3)

#include <Adafruit_NeoPixel.h>
#include <LittleFS.h>

// Cast helper (strip stored as void* to avoid header dependency in .h)
#define STRIP ((Adafruit_NeoPixel*)_strip)

void PixelMapper::begin() {
  loadConfig();
  if (_cfg.enabled && _cfg.ledCount > 0) {
    auto* s = new Adafruit_NeoPixel(_cfg.ledCount, PIXEL_STRIP_PIN, NEO_GRB + NEO_KHZ800);
    s->begin();
    s->setBrightness(_cfg.brightness);
    s->clear();
    s->show();
    _strip = s;
    Serial.printf("[PIX] LED trak inicializiran: %d LED-ic na GPIO%d\n",
                   _cfg.ledCount, PIXEL_STRIP_PIN);
  }
}

void PixelMapper::setConfig(const PixelMapConfig& cfg) {
  bool needsReinit = (_cfg.ledCount != cfg.ledCount || _cfg.enabled != cfg.enabled);
  _cfg = cfg;

  if (needsReinit) {
    if (_strip) { STRIP->clear(); STRIP->show(); delete STRIP; _strip = nullptr; }
    if (_cfg.enabled && _cfg.ledCount > 0) {
      auto* s = new Adafruit_NeoPixel(_cfg.ledCount, PIXEL_STRIP_PIN, NEO_GRB + NEO_KHZ800);
      s->begin();
      s->setBrightness(_cfg.brightness);
      s->clear();
      s->show();
      _strip = s;
    }
  } else if (_strip) {
    STRIP->setBrightness(_cfg.brightness);
  }
}

void PixelMapper::update(const uint8_t* dmxOut, FixtureEngine* fixtures,
                          SoundEngine* sound, float dt) {
  if (!_strip || !_cfg.enabled) return;

  switch (_cfg.mode) {
    case PXMAP_FIXTURE:  modeFixture(dmxOut, fixtures); break;
    case PXMAP_VU_METER: modeVuMeter(sound);            break;
    case PXMAP_SPECTRUM: modeSpectrum(sound);            break;
    case PXMAP_PULSE:    modePulse(sound, dt);           break;
    default: break;
  }

  STRIP->show();  // Uses RMT peripheral — zero CPU load
}

// ── Mode: Fixture Mirror ──
// Mirrors R/G/B color from a fixture (or first fixture in group) to entire strip
void PixelMapper::modeFixture(const uint8_t* dmxOut, FixtureEngine* fixtures) {
  if (!fixtures) return;

  // Collect fixture indices to mirror
  int targets[MAX_FIXTURES];
  int targetCount = 0;

  if (_cfg.groupMask) {
    for (int i = 0; i < MAX_FIXTURES && targetCount < MAX_FIXTURES; i++) {
      const PatchEntry* fx = fixtures->getFixture(i);
      if (fx && fx->active && (fx->groupMask & _cfg.groupMask))
        targets[targetCount++] = i;
    }
  } else {
    targets[0] = _cfg.fixtureIdx;
    targetCount = 1;
  }

  if (targetCount == 0) { STRIP->clear(); return; }

  int ledsPerFx = _cfg.ledCount / targetCount;
  if (ledsPerFx < 1) ledsPerFx = 1;

  for (int t = 0; t < targetCount; t++) {
    const PatchEntry* fx = fixtures->getFixture(targets[t]);
    if (!fx || !fx->active) continue;

    // Extract RGB from DMX output
    uint8_t r = 0, g = 0, b = 0;
    uint8_t chCount = fixtures->fixtureChannelCount(targets[t]);
    for (int c = 0; c < chCount; c++) {
      const ChannelDef* cd = fixtures->fixtureChannel(targets[t], c);
      if (!cd) continue;
      uint16_t addr = fx->dmxAddress + c - 1;
      if (addr >= DMX_MAX_CHANNELS) continue;
      uint8_t val = dmxOut[addr];
      switch (cd->type) {
        case CH_COLOR_R: r = val; break;
        case CH_COLOR_G: g = val; break;
        case CH_COLOR_B: b = val; break;
      }
    }

    // Apply dimmer if present
    for (int c = 0; c < chCount; c++) {
      const ChannelDef* cd = fixtures->fixtureChannel(targets[t], c);
      if (cd && cd->type == CH_INTENSITY) {
        uint16_t addr = fx->dmxAddress + c - 1;
        if (addr < DMX_MAX_CHANNELS) {
          uint8_t dim = dmxOut[addr];
          r = (uint16_t)r * dim / 255;
          g = (uint16_t)g * dim / 255;
          b = (uint16_t)b * dim / 255;
        }
        break;
      }
    }

    uint32_t color = STRIP->Color(r, g, b);
    int startLed = t * ledsPerFx;
    int endLed = (t + 1 < targetCount) ? (t + 1) * ledsPerFx : _cfg.ledCount;
    for (int i = startLed; i < endLed; i++) STRIP->setPixelColor(i, color);
  }
}

// ── Mode: VU Meter ──
// Green (bass) → Yellow (mid) → Red (high) like a classic VU meter
void PixelMapper::modeVuMeter(SoundEngine* sound) {
  if (!sound) { STRIP->clear(); return; }
  const FFTBands& bands = sound->getBands();

  float level = (bands.bass + bands.mid + bands.high) / 3.0f;
  int litLeds = (int)(level * _cfg.ledCount);

  for (int i = 0; i < (int)_cfg.ledCount; i++) {
    if (i >= litLeds) { STRIP->setPixelColor(i, 0); continue; }
    float pos = (float)i / _cfg.ledCount;
    uint8_t r, g, b;
    if (pos < 0.5f) {
      // Green → Yellow
      float t = pos * 2.0f;
      r = (uint8_t)(t * 255); g = 255; b = 0;
    } else {
      // Yellow → Red
      float t = (pos - 0.5f) * 2.0f;
      r = 255; g = (uint8_t)((1.0f - t) * 255); b = 0;
    }
    STRIP->setPixelColor(i, STRIP->Color(r, g, b));
  }
}

// ── Mode: Spectrum ──
// Each LED = one FFT frequency bin, mapped rainbow (low=red → high=violet)
void PixelMapper::modeSpectrum(SoundEngine* sound) {
  if (!sound) { STRIP->clear(); return; }
  const FFTBands& bands = sound->getBands();

  // Map 8 bands across ledCount
  for (int i = 0; i < (int)_cfg.ledCount; i++) {
    float pos = (float)i / _cfg.ledCount;
    int bandIdx = (int)(pos * STL_BAND_COUNT);
    if (bandIdx >= STL_BAND_COUNT) bandIdx = STL_BAND_COUNT - 1;

    float val = bands.bands[bandIdx];
    // Rainbow hue based on position
    uint16_t hue = (uint16_t)(pos * 65535);
    uint8_t sat = 255;
    uint8_t bright = (uint8_t)(val * 255);
    STRIP->setPixelColor(i, STRIP->ColorHSV(hue, sat, bright));
  }
}

// ── Mode: Beat Pulse ──
// All LEDs flash on beat with color cycling
void PixelMapper::modePulse(SoundEngine* sound, float dt) {
  if (!sound) { STRIP->clear(); return; }
  const FFTBands& bands = sound->getBands();

  // Beat pulse: ramp up on beat, decay over time
  if (bands.beatDetected) _pulsePhase = 1.0f;
  _pulsePhase *= (1.0f - dt * 5.0f);  // Decay ~200ms
  if (_pulsePhase < 0.01f) _pulsePhase = 0;

  // Rotate hue slowly
  static float hue = 0;
  hue += dt * 30.0f;  // 30 degrees/sec
  if (hue >= 360.0f) hue -= 360.0f;
  uint16_t hue16 = (uint16_t)(hue / 360.0f * 65535);

  uint8_t bright = (uint8_t)(_pulsePhase * 255);
  uint32_t color = Adafruit_NeoPixel::ColorHSV(hue16, 255, bright);
  for (int i = 0; i < (int)_cfg.ledCount; i++)
    STRIP->setPixelColor(i, color);
}

// ── Persistence ──
void PixelMapper::saveConfig() {
  File f = LittleFS.open("/pixmap.bin", "w");
  if (!f) return;
  uint8_t ver = 1;
  f.write(&ver, 1);
  f.write((uint8_t*)&_cfg, sizeof(PixelMapConfig));
  f.close();
  Serial.println("[PIX] Konfiguracija shranjena");
}

void PixelMapper::loadConfig() {
  File f = LittleFS.open("/pixmap.bin", "r");
  if (!f) return;
  uint8_t ver;
  if (f.read(&ver, 1) == 1 && ver == 1) {
    f.read((uint8_t*)&_cfg, sizeof(PixelMapConfig));
  }
  f.close();
  Serial.println("[PIX] Konfiguracija nalozena");
}

#endif // CONFIG_IDF_TARGET_ESP32S3
