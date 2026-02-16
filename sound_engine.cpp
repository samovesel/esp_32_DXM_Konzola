#include "sound_engine.h"
#include <arduinoFFT.h>
#include <math.h>
#include <LittleFS.h>

// Frekvenčne meje za 8 vizualizacijskih pasov
static const uint16_t BAND_EDGES[STL_BAND_COUNT + 1] = {
  30, 60, 120, 250, 500, 1000, 2000, 4000, 11000
};

void SoundEngine::begin(AudioInput* audio, FixtureEngine* fixtures) {
  _audio = audio;
  _fixtures = fixtures;
  _easy = STL_EASY_DEFAULTS;
  memset(_rules, 0, sizeof(_rules));
  memset(&_bands, 0, sizeof(_bands));
  memset(_beatHistory, 0, sizeof(_beatHistory));
  memset(_ruleValues, 0, sizeof(_ruleValues));
  memset(_fxLevels, 0, sizeof(_fxLevels));
  _smoothBass = 0; _smoothMid = 0; _smoothHigh = 0; _smoothBeat = 0;
  _hueAngle = 0;
  _beatPhase = 0;
  _lastUpdateTime = millis();
  loadConfig();
  Serial.println("[SND] Sound engine inicializiran");
}

void SoundEngine::update() {
  if (!_audio || !_audio->isRunning()) return;
  if (!_audio->samplesReady()) return;

  float* raw = _audio->getSamples();
  memcpy(_vReal, raw, sizeof(float) * FFT_SAMPLES);
  memset(_vImag, 0, sizeof(float) * FFT_SAMPLES);
  _audio->consumeSamples();

  unsigned long now = millis();
  float dt = (now - _lastUpdateTime) / 1000.0f;
  _lastUpdateTime = now;
  if (dt <= 0 || dt > 1.0f) dt = 0.05f;

  processFFT();
  extractBands(dt);
  detectBeat(dt);
  updateBeatSync(dt);
}

// ============================================================================
//  FFT
// ============================================================================

void SoundEngine::processFFT() {
  ArduinoFFT<float> fft(_vReal, _vImag, FFT_SAMPLES, FFT_SAMPLE_RATE);
  fft.windowing(FFTWindow::Hamming, FFTDirection::Forward);
  fft.compute(FFTDirection::Forward);
  fft.complexToMagnitude();
}

// ============================================================================
//  FREKVENČNI PASOVI
// ============================================================================

void SoundEngine::extractBands(float dt) {
  float freqPerBin = (float)FFT_SAMPLE_RATE / FFT_SAMPLES;

  for (int b = 0; b < STL_BAND_COUNT; b++) {
    int binLow  = (int)(BAND_EDGES[b] / freqPerBin);
    int binHigh = (int)(BAND_EDGES[b + 1] / freqPerBin);
    if (binLow < 1) binLow = 1;
    if (binHigh >= FFT_BINS) binHigh = FFT_BINS - 1;

    float sum = 0;
    int count = 0;
    for (int i = binLow; i <= binHigh; i++) { sum += _vReal[i]; count++; }
    float avg = count > 0 ? sum / count : 0;

    float normalized = fminf(avg * _easy.sensitivity / 200.0f, 1.0f);
    _bands.bands[b] = smoothValue(_bands.bands[b], normalized, 30, 120, dt);
  }

  // Bass/Mid/High povprečja
  float rawBass = (_bands.bands[0] + _bands.bands[1] + _bands.bands[2]) / 3.0f;
  float rawMid  = (_bands.bands[3] + _bands.bands[4] + _bands.bands[5]) / 3.0f;
  float rawHigh = (_bands.bands[6] + _bands.bands[7]) / 2.0f;

  _smoothBass = smoothValue(_smoothBass, rawBass, 20, 150, dt);
  _smoothMid  = smoothValue(_smoothMid,  rawMid,  30, 200, dt);
  _smoothHigh = smoothValue(_smoothHigh, rawHigh, 15, 100, dt);

  _bands.bass = _smoothBass;
  _bands.mid  = _smoothMid;
  _bands.high = _smoothHigh;
}

// ============================================================================
//  BEAT DETEKCIJA
// ============================================================================

void SoundEngine::detectBeat(float dt) {
  float energy = _bands.bands[0] + _bands.bands[1];
  _beatHistory[_beatHistIdx] = energy;
  _beatHistIdx = (_beatHistIdx + 1) % BEAT_HISTORY_SIZE;

  float avg = 0;
  for (int i = 0; i < BEAT_HISTORY_SIZE; i++) avg += _beatHistory[i];
  avg /= BEAT_HISTORY_SIZE;

  float threshold = avg * 1.4f + 0.05f;
  unsigned long now = millis();
  bool beat = false;

  if (energy > threshold && (now - _lastBeatTime > 200)) {
    beat = true;

    if (_lastBeatTime > 0) {
      float interval = now - _lastBeatTime;
      if (interval > 250 && interval < 2000) {
        float bpm = 60000.0f / interval;
        _bpmAccum += bpm;
        _bpmCount++;
        if (_bpmCount >= 8) {
          _bands.bpm = _bpmAccum / _bpmCount;
          _bpmAccum = 0;
          _bpmCount = 0;
          // Posodobi beat interval za sync
          _beatIntervalMs = interval;
        }
      }
    }
    _lastBeatTime = now;
  }

  _bands.beatDetected = beat;

  if (beat) _smoothBeat = 1.0f;
  else {
    _smoothBeat = smoothValue(_smoothBeat, 0, 10, 300, dt);
  }
}

// ============================================================================
//  BEAT SYNC FAZA
// ============================================================================

void SoundEngine::updateBeatSync(float dt) {
  if (!_easy.beatSync || _beatIntervalMs < 200) {
    _beatPhase = 0;
    _easy.beatPhase = 0;
    return;
  }

  unsigned long now = millis();
  float elapsed = (float)(now - _lastBeatMs);

  // Ob beatu resetiraj fazo
  if (_bands.beatDetected) {
    _lastBeatMs = now;
    _beatPhase = 0;
  } else {
    _beatPhase = fminf(elapsed / _beatIntervalMs, 1.0f);
  }
  _easy.beatPhase = _beatPhase;
}

// ============================================================================
//  ZONE - energija per-fixture
// ============================================================================

float SoundEngine::getZoneEnergy(SoundZone zone) const {
  switch (zone) {
    case ZONE_BASS: return _smoothBass;
    case ZONE_MID:  return _smoothMid;
    case ZONE_HIGH: return _smoothHigh;
    default:        return (_smoothBass + _smoothMid + _smoothHigh) / 3.0f;
  }
}

// ============================================================================
//  PRESETI
// ============================================================================

void SoundEngine::applyPreset(uint8_t preset) {
  _easy.preset = preset;
  switch ((SoundPreset)preset) {
    case SPRESET_PULSE:
      _easy.bassIntensity = true;  _easy.midColor = false;
      _easy.highStrobe = false;    _easy.beatBump = true;
      _easy.soundAmount = 0.8f;    _easy.beatSync = false;
      break;
    case SPRESET_RAINBOW:
      _easy.bassIntensity = false; _easy.midColor = true;
      _easy.highStrobe = false;    _easy.beatBump = false;
      _easy.soundAmount = 0.7f;    _easy.beatSync = true;
      break;
    case SPRESET_STORM:
      _easy.bassIntensity = false; _easy.midColor = false;
      _easy.highStrobe = true;     _easy.beatBump = true;
      _easy.soundAmount = 1.0f;    _easy.beatSync = true;
      break;
    case SPRESET_AMBIENT:
      _easy.bassIntensity = true;  _easy.midColor = true;
      _easy.highStrobe = false;    _easy.beatBump = false;
      _easy.soundAmount = 0.3f;    _easy.beatSync = false;
      break;
    case SPRESET_CLUB:
      _easy.bassIntensity = true;  _easy.midColor = true;
      _easy.highStrobe = true;     _easy.beatBump = true;
      _easy.soundAmount = 0.6f;    _easy.beatSync = true;
      break;
    default: break;  // CUSTOM — ne spreminjaj
  }
}

// ============================================================================
//  APLICIRANJE NA DMX
// ============================================================================

void SoundEngine::applyToOutput(const uint8_t* manualValues, uint8_t* dmxOut, float dt) {
  if (!isActive()) return;
  if (dt <= 0) dt = 0.05f;

  if (_easy.enabled) applyEasyMode(manualValues, dmxOut, dt);
  applyProMode(manualValues, dmxOut, dt);
}

void SoundEngine::applyEasyMode(const uint8_t* manualValues, uint8_t* dmxOut, float dt) {
  // HUE rotacija za rainbow
  if (_easy.midColor) {
    float rotSpeed = _easy.beatSync ? (_beatPhase * 360.0f) : (dt * 60.0f);
    if (!_easy.beatSync) _hueAngle = fmodf(_hueAngle + rotSpeed, 360.0f);
  }

  for (int fi = 0; fi < MAX_FIXTURES; fi++) {
    const PatchEntry* fx = _fixtures->getFixture(fi);
    if (!fx || !fx->active || fx->profileIndex < 0) continue;
    // FIX: uporabi soundReactive flag iz patcha
    if (!fx->soundReactive) continue;

    uint8_t chCount = _fixtures->fixtureChannelCount(fi);
    float amount = _easy.soundAmount;
    SoundZone zone = (SoundZone)_easy.zones[fi];
    float zoneE = getZoneEnergy(zone);
    float fxLevel = 0;

    for (int ch = 0; ch < chCount; ch++) {
      const ChannelDef* def = _fixtures->fixtureChannel(fi, ch);
      if (!def) continue;
      uint16_t addr = fx->dmxAddress + ch - 1;
      if (addr >= DMX_MAX_CHANNELS) continue;

      float modifier = 0;

      // Bass → Intensity (dimmer)
      if (_easy.bassIntensity && def->type == CH_INTENSITY) {
        float bassE = (zone == ZONE_ALL) ? _smoothBass : zoneE;
        modifier = bassE;
        if (_easy.beatBump) modifier = fminf(modifier + _smoothBeat * 0.5f, 1.0f);
        // Beat sync: pulziranje
        if (_easy.beatSync && _beatIntervalMs > 200) {
          float pulse = 1.0f - _beatPhase; // Pojenja od beata
          modifier *= (0.3f + 0.7f * pulse);
        }
      }

      // Mid → Color shift
      if (_easy.midColor) {
        float midE = (zone == ZONE_ALL) ? _smoothMid : zoneE;
        float hue = _easy.beatSync ? (_beatPhase * 360.0f) : _hueAngle;
        // Offset po fixture indexu za raznolikost
        hue = fmodf(hue + fi * 45.0f, 360.0f);
        // HSV → RGB aproksimacija
        float h6 = hue / 60.0f;
        float frac = h6 - floorf(h6);
        if (def->type == CH_COLOR_R) {
          if (h6 < 1) modifier = 1.0f;
          else if (h6 < 2) modifier = 1.0f - frac;
          else if (h6 < 4) modifier = 0;
          else if (h6 < 5) modifier = frac;
          else modifier = 1.0f;
        } else if (def->type == CH_COLOR_G) {
          if (h6 < 1) modifier = frac;
          else if (h6 < 3) modifier = 1.0f;
          else if (h6 < 4) modifier = 1.0f - frac;
          else modifier = 0;
        } else if (def->type == CH_COLOR_B) {
          if (h6 < 2) modifier = 0;
          else if (h6 < 3) modifier = frac;
          else if (h6 < 5) modifier = 1.0f;
          else modifier = 1.0f - frac;
        }
        modifier *= midE;
      }

      // High → Strobe
      if (_easy.highStrobe && (def->type == CH_STROBE || def->type == CH_SHUTTER)) {
        float highE = (zone == ZONE_ALL) ? _smoothHigh : zoneE;
        modifier = highE;
        // Beat sync: strobe samo na beat
        if (_easy.beatSync && _smoothBeat < 0.3f) modifier *= 0.1f;
      }

      if (modifier > 0) {
        fxLevel = fmaxf(fxLevel, modifier);
        float base = (float)manualValues[addr];
        float sound = modifier * 255.0f;
        float result = base + sound * amount;
        if (result > 255.0f) result = 255.0f;
        dmxOut[addr] = (uint8_t)result;
      }
    }
    _fxLevels[fi] = fxLevel;
  }
}

void SoundEngine::applyProMode(const uint8_t* manualValues, uint8_t* dmxOut, float dt) {
  float freqPerBin = (float)FFT_SAMPLE_RATE / FFT_SAMPLES;

  for (int r = 0; r < STL_MAX_RULES; r++) {
    if (!_rules[r].active) continue;
    const STLRule& rule = _rules[r];

    const PatchEntry* fx = _fixtures->getFixture(rule.fixtureIdx);
    if (!fx || !fx->active) continue;

    int binLow  = (int)(rule.freqLow / freqPerBin);
    int binHigh = (int)(rule.freqHigh / freqPerBin);
    if (binLow < 1) binLow = 1;
    if (binHigh >= FFT_BINS) binHigh = FFT_BINS - 1;

    float sum = 0;
    int count = 0;
    for (int i = binLow; i <= binHigh; i++) { sum += _vReal[i]; count++; }
    float energy = count > 0 ? sum / count : 0;
    float normalized = fminf(energy * _easy.sensitivity / 300.0f, 1.0f);
    normalized = applyResponseCurve(normalized, (ResponseCurve)rule.curve);

    // FIX: uporabi dejanski dt namesto hardkodiranega
    float attackMs = rule.attackMs * 10.0f;
    float decayMs  = rule.decayMs * 10.0f;
    _ruleValues[r] = smoothValue(_ruleValues[r], normalized, attackMs, decayMs, dt);

    float v = _ruleValues[r];
    uint8_t dmxVal = rule.outMin + (uint8_t)(v * (rule.outMax - rule.outMin));

    uint16_t addr = fx->dmxAddress + rule.channelIdx - 1;
    if (addr < DMX_MAX_CHANNELS) {
      dmxOut[addr] = dmxVal;
    }
  }
}

bool SoundEngine::isActive() const {
  if (_easy.enabled) return true;
  for (int i = 0; i < STL_MAX_RULES; i++) {
    if (_rules[i].active) return true;
  }
  return false;
}

// ============================================================================
//  PRO MODE PRAVILA
// ============================================================================

const STLRule* SoundEngine::getRule(int idx) const {
  if (idx < 0 || idx >= STL_MAX_RULES) return nullptr;
  return _rules[idx].active ? &_rules[idx] : nullptr;
}

bool SoundEngine::setRule(int idx, const STLRule& rule) {
  if (idx < 0 || idx >= STL_MAX_RULES) return false;
  _rules[idx] = rule;
  _rules[idx].active = true;
  return true;
}

bool SoundEngine::clearRule(int idx) {
  if (idx < 0 || idx >= STL_MAX_RULES) return false;
  _rules[idx].active = false;
  _ruleValues[idx] = 0;
  return true;
}

int SoundEngine::getRuleCount() const {
  int count = 0;
  for (int i = 0; i < STL_MAX_RULES; i++) {
    if (_rules[i].active) count++;
  }
  return count;
}

// ============================================================================
//  PERSISTENCA
// ============================================================================

#define SND_MAGIC 0xAE

void SoundEngine::saveConfig() {
  File f = LittleFS.open(PATH_SOUND_CFG, "w");
  if (!f) { Serial.println("[SND] Napaka pri pisanju"); return; }
  uint8_t magic = SND_MAGIC;
  f.write(&magic, 1);
  f.write((uint8_t*)&_easy, sizeof(STLEasyConfig));
  f.write((uint8_t*)_rules, sizeof(_rules));
  f.close();
  Serial.println("[SND] Konfiguracija shranjena");
}

void SoundEngine::loadConfig() {
  File f = LittleFS.open(PATH_SOUND_CFG, "r");
  if (!f) return;
  uint8_t magic = 0;
  f.read(&magic, 1);
  if (magic != SND_MAGIC) { f.close(); return; }
  if (f.read((uint8_t*)&_easy, sizeof(STLEasyConfig)) != sizeof(STLEasyConfig)) {
    _easy = STL_EASY_DEFAULTS;
  }
  f.read((uint8_t*)_rules, sizeof(_rules));
  f.close();
  Serial.println("[SND] Konfiguracija naložena");
}

// ============================================================================
//  POMOŽNE
// ============================================================================

float SoundEngine::applyResponseCurve(float input, ResponseCurve curve) {
  if (input <= 0) return 0;
  if (input >= 1.0f) return 1.0f;
  switch (curve) {
    case CURVE_EXPONENTIAL: return input * input;
    case CURVE_LOGARITHMIC: return logf(1.0f + input * 9.0f) / logf(10.0f);
    case CURVE_SQUARE:      return sqrtf(input);
    default:                return input;
  }
}

float SoundEngine::smoothValue(float current, float target, float attackMs, float decayMs, float dt) {
  if (target > current) {
    float rate = attackMs > 0 ? dt / (attackMs / 1000.0f) : 1.0f;
    if (rate > 1.0f) rate = 1.0f;
    return current + (target - current) * rate;
  } else {
    float rate = decayMs > 0 ? dt / (decayMs / 1000.0f) : 1.0f;
    if (rate > 1.0f) rate = 1.0f;
    return current - (current - target) * rate;
  }
}
