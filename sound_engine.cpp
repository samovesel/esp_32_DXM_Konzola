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
  _mbCfg = MANUAL_BEAT_DEFAULTS;
  memset(_rules, 0, sizeof(_rules));
  memset(&_bands, 0, sizeof(_bands));
  memset(_beatHistory, 0, sizeof(_beatHistory));
  memset(_ruleValues, 0, sizeof(_ruleValues));
  memset(_fxLevels, 0, sizeof(_fxLevels));
  memset(_tapTimes, 0, sizeof(_tapTimes));
  memset(_mbRandomHues, 0, sizeof(_mbRandomHues));
  _smoothBass = 0; _smoothMid = 0; _smoothHigh = 0; _smoothBeat = 0;
  _hueAngle = 0;
  _beatPhase = 0;
  _tapCount = 0; _lastTapMs = 0;
  _mbPhase = 0; _mbLastBeatMs = 0; _mbBeatCount = 0; _mbSmoothBeat = 0;
  _mbChaseIdx = 0; _mbStackCount = 0; _mbScanDir = 1; _mbScanIdx = 0;
  _mbTransition = 0; _mbAudioPresent = false;
  _lastUpdateTime = millis();
  loadConfig();
  Serial.println("[SND] Sound engine inicializiran");
}

void SoundEngine::update() {
  unsigned long now = millis();
  float dt = (now - _lastUpdateTime) / 1000.0f;
  if (dt <= 0 || dt > 1.0f) dt = 0.05f;

  bool audioProcessed = false;
  if (_audio && _audio->isRunning() && _audio->samplesReady()) {
    float* raw = _audio->getSamples();
    memcpy(_vReal, raw, sizeof(float) * FFT_SAMPLES);
    memset(_vImag, 0, sizeof(float) * FFT_SAMPLES);
    _audio->consumeSamples();

    _lastUpdateTime = now;

    processFFT();
    extractBands(dt);
    detectBeat(dt);
    updateBeatSync(dt);
    audioProcessed = true;
  }

  // Preveri ali je avdio signal prisoten (za AUTO fallback)
  if (audioProcessed) {
    float peak = (_audio && _audio->isRunning()) ? _audio->getPeakLevel() : 0;
    _mbAudioPresent = (peak > 0.02f); // Prag za tišino
  } else if (now - _lastUpdateTime > 2000) {
    _mbAudioPresent = false; // Ni bilo avdio podatkov > 2s
  }

  // Manual beat se posodablja vedno (neodvisno od avdia)
  if (_mbCfg.enabled) {
    _lastUpdateTime = now;
    updateManualBeat(dt);
  }
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

  // Manual beat program — deluje neodvisno ali skupaj z easy mode
  if (_mbCfg.enabled && isManualBeatActive()) {
    applyManualBeatProgram(manualValues, dmxOut, dt);
  }
  // Easy mode (audio-driven) — ko je manual v AUTO načinu in signal prisoten, ali ko je samo easy mode
  if (_easy.enabled && (!_mbCfg.enabled || !isManualBeatActive())) {
    applyEasyMode(manualValues, dmxOut, dt);
  }
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
  if (_mbCfg.enabled) return true;
  for (int i = 0; i < STL_MAX_RULES; i++) {
    if (_rules[i].active) return true;
  }
  return false;
}

bool SoundEngine::isManualBeatActive() const {
  if (!_mbCfg.enabled) return false;
  BeatSource src = (BeatSource)_mbCfg.source;
  if (src == BSRC_MANUAL) return true;
  if (src == BSRC_AUTO && !_mbAudioPresent) return true;
  return false;
}

// ============================================================================
//  MANUAL BEAT — tap tempo, faza, programi
// ============================================================================

float SoundEngine::getSubdivMultiplier() const {
  switch ((BeatSubdiv)_mbCfg.subdiv) {
    case SUBDIV_QUARTER: return 0.25f;
    case SUBDIV_HALF:    return 0.5f;
    case SUBDIV_DOUBLE:  return 2.0f;
    case SUBDIV_QUAD:    return 4.0f;
    default:             return 1.0f;
  }
}

float SoundEngine::getEffectiveBpm() const {
  return _mbCfg.bpm * getSubdivMultiplier();
}

int SoundEngine::countSoundReactiveFixtures() const {
  int count = 0;
  for (int i = 0; i < MAX_FIXTURES; i++) {
    const PatchEntry* fx = _fixtures->getFixture(i);
    if (fx && fx->active && fx->profileIndex >= 0 && fx->soundReactive) count++;
  }
  return count;
}

void SoundEngine::tapBeat() {
  unsigned long now = millis();

  // Reset po 2 sekundah brez tapa
  if (_tapCount > 0 && (now - _lastTapMs) > 2000) {
    _tapCount = 0;
  }

  _tapTimes[_tapCount % 4] = now;
  _tapCount++;
  _lastTapMs = now;

  // Po 4 tapih izračunaj BPM
  if (_tapCount >= 4) {
    int idx = (_tapCount - 1) % 4;
    int oldest = (_tapCount - 4) % 4;
    float totalMs = (float)(_tapTimes[idx] - _tapTimes[oldest]);
    float avgInterval = totalMs / 3.0f;
    if (avgInterval > 150 && avgInterval < 3000) {
      _mbCfg.bpm = 60000.0f / avgInterval;
      if (_mbCfg.bpm < 30) _mbCfg.bpm = 30;
      if (_mbCfg.bpm > 300) _mbCfg.bpm = 300;
    }
  }

  // Vsak tap sproži beat
  _mbPhase = 0;
  _mbLastBeatMs = now;
  _mbSmoothBeat = 1.0f;
}

void SoundEngine::setManualBpm(float bpm) {
  if (bpm < 30) bpm = 30;
  if (bpm > 300) bpm = 300;
  _mbCfg.bpm = bpm;
}

void SoundEngine::updateManualBeat(float dt) {
  if (!_mbCfg.enabled) return;

  float effectiveBpm = getEffectiveBpm();
  float intervalMs = 60000.0f / effectiveBpm;
  if (intervalMs < 100) intervalMs = 100;

  unsigned long now = millis();
  float elapsed = (float)(now - _mbLastBeatMs);

  // Izračunaj fazo
  _mbPhase = fminf(elapsed / intervalMs, 1.0f);

  // Ob novem beatu
  if (_mbPhase >= 1.0f) {
    _mbPhase = 0;
    _mbLastBeatMs = now;
    _mbSmoothBeat = 1.0f;
    _mbBeatCount++;

    // Posodobi chase/scanner/stack indekse
    int srCount = countSoundReactiveFixtures();
    if (srCount > 0) {
      _mbChaseIdx = (_mbChaseIdx + 1) % srCount;

      // Scanner — obrne smer na koncih
      _mbScanIdx += _mbScanDir;
      if (_mbScanIdx >= srCount - 1) { _mbScanIdx = srCount - 1; _mbScanDir = -1; }
      if (_mbScanIdx <= 0) { _mbScanIdx = 0; _mbScanDir = 1; }

      // Stack
      _mbStackCount++;
      if (_mbStackCount > srCount) _mbStackCount = 0;

      // Random barve
      for (int i = 0; i < MAX_FIXTURES; i++) {
        _mbRandomHues[i] = (float)(random(0, 360));
      }
    }
  } else {
    // Decay smooth beat
    _mbSmoothBeat = smoothValue(_mbSmoothBeat, 0, 10, 300, dt);
  }

  // Posodobi tudi globalni _beatPhase če manual beat poganja
  if (isManualBeatActive()) {
    _beatPhase = _mbPhase;
    _beatIntervalMs = intervalMs;
    _bands.bpm = _mbCfg.bpm;
  }
}

void SoundEngine::applyManualBeatProgram(const uint8_t* manualValues, uint8_t* dmxOut, float dt) {
  ManualBeatProgram prog = (ManualBeatProgram)_mbCfg.program;
  float amount = _mbCfg.intensity;
  float phase = _mbPhase;
  int srCount = countSoundReactiveFixtures();
  if (srCount == 0) return;

  // Zgradi seznam sound-reactive fixture indeksov
  int srFixtures[MAX_FIXTURES];
  int srN = 0;
  for (int i = 0; i < MAX_FIXTURES; i++) {
    const PatchEntry* fx = _fixtures->getFixture(i);
    if (fx && fx->active && fx->profileIndex >= 0 && fx->soundReactive) {
      srFixtures[srN++] = i;
    }
  }

  for (int si = 0; si < srN; si++) {
    int fi = srFixtures[si];
    const PatchEntry* fx = _fixtures->getFixture(fi);
    if (!fx) continue;
    uint8_t chCount = _fixtures->fixtureChannelCount(fi);

    float dimMod = 0;     // Intenziteta modifikator 0-1
    float colorHue = -1;  // -1 = brez barvne spremembe; 0-360 = hue

    // Izračunaj modifikator glede na program
    switch (prog) {
      case MBPROG_PULSE: {
        dimMod = 1.0f - phase; // Pojenja od beata
        break;
      }
      case MBPROG_CHASE: {
        dimMod = (si == _mbChaseIdx) ? 1.0f : 0.05f;
        break;
      }
      case MBPROG_SINE: {
        dimMod = 0.5f + 0.5f * sinf(phase * 2.0f * M_PI);
        break;
      }
      case MBPROG_STROBE: {
        dimMod = (phase < 0.15f) ? 1.0f : 0.0f;
        break;
      }
      case MBPROG_RAINBOW: {
        dimMod = 0.6f + 0.4f * (1.0f - phase); // Rahel pulse
        colorHue = fmodf(phase * 360.0f + si * (360.0f / srN), 360.0f);
        break;
      }
      case MBPROG_BUILD: {
        int beats = _mbCfg.buildBeats;
        if (beats < 2) beats = 8;
        int pos = _mbBeatCount % beats;
        float progress = (float)pos / (float)(beats - 1);
        dimMod = progress * (1.0f - phase * 0.3f); // Gradnja z rahlim pulsom
        if (pos == beats - 1 && phase < 0.1f) dimMod = 1.0f; // Flash na vrhu
        break;
      }
      case MBPROG_RANDOM: {
        dimMod = 0.3f + 0.7f * _mbSmoothBeat; // Flash ob beatu
        colorHue = _mbRandomHues[fi];
        break;
      }
      case MBPROG_ALTERNATE: {
        bool even = (_mbBeatCount % 2 == 0);
        bool isEven = (si % 2 == 0);
        dimMod = ((even && isEven) || (!even && !isEven)) ? (1.0f - phase * 0.5f) : 0.05f;
        break;
      }
      case MBPROG_WAVE: {
        float offset = (float)si / (float)srN;
        float wavePhase = fmodf(phase + offset, 1.0f);
        dimMod = 0.5f + 0.5f * sinf(wavePhase * 2.0f * M_PI);
        break;
      }
      case MBPROG_STACK: {
        dimMod = (si < _mbStackCount) ? (0.7f + 0.3f * _mbSmoothBeat) : 0.0f;
        break;
      }
      case MBPROG_SPARKLE: {
        // Naključen fixture utripa ob beatu
        if (_mbSmoothBeat > 0.5f) {
          dimMod = (random(0, srN) == si) ? 1.0f : 0.1f;
        } else {
          dimMod = 0.05f;
        }
        break;
      }
      case MBPROG_SCANNER: {
        dimMod = (si == _mbScanIdx) ? 1.0f : 0.05f;
        break;
      }
      default:
        dimMod = 1.0f - phase;
        break;
    }

    float fxLevel = 0;

    // Apliciraj na DMX kanale
    for (int ch = 0; ch < chCount; ch++) {
      const ChannelDef* def = _fixtures->fixtureChannel(fi, ch);
      if (!def) continue;
      uint16_t addr = fx->dmxAddress + ch - 1;
      if (addr >= DMX_MAX_CHANNELS) continue;

      float modifier = 0;

      // Intensity kanali
      if (def->type == CH_INTENSITY) {
        modifier = dimMod;
      }

      // Barvni kanali
      if (_mbCfg.colorEnabled && colorHue >= 0) {
        float h6 = colorHue / 60.0f;
        float frac = h6 - floorf(h6);
        if (def->type == CH_COLOR_R) {
          if (h6 < 1) modifier = 1.0f;
          else if (h6 < 2) modifier = 1.0f - frac;
          else if (h6 < 4) modifier = 0;
          else if (h6 < 5) modifier = frac;
          else modifier = 1.0f;
          modifier *= dimMod;
        } else if (def->type == CH_COLOR_G) {
          if (h6 < 1) modifier = frac;
          else if (h6 < 3) modifier = 1.0f;
          else if (h6 < 4) modifier = 1.0f - frac;
          else modifier = 0;
          modifier *= dimMod;
        } else if (def->type == CH_COLOR_B) {
          if (h6 < 2) modifier = 0;
          else if (h6 < 3) modifier = frac;
          else if (h6 < 5) modifier = 1.0f;
          else modifier = 1.0f - frac;
          modifier *= dimMod;
        }
      }

      // Strobe kanali — strobe program jih aktivira
      if ((def->type == CH_STROBE || def->type == CH_SHUTTER) && prog == MBPROG_STROBE) {
        modifier = dimMod;
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

#define SND_MAGIC    0xAE
#define SND_MAGIC_V2 0xAF  // V2: vključuje ManualBeatConfig

void SoundEngine::saveConfig() {
  File f = LittleFS.open(PATH_SOUND_CFG, "w");
  if (!f) { Serial.println("[SND] Napaka pri pisanju"); return; }
  uint8_t magic = SND_MAGIC_V2;
  f.write(&magic, 1);
  f.write((uint8_t*)&_easy, sizeof(STLEasyConfig));
  f.write((uint8_t*)_rules, sizeof(_rules));
  f.write((uint8_t*)&_mbCfg, sizeof(ManualBeatConfig));
  f.close();
  Serial.println("[SND] Konfiguracija shranjena (V2)");
}

void SoundEngine::loadConfig() {
  File f = LittleFS.open(PATH_SOUND_CFG, "r");
  if (!f) return;
  uint8_t magic = 0;
  f.read(&magic, 1);
  if (magic != SND_MAGIC && magic != SND_MAGIC_V2) { f.close(); return; }
  if (f.read((uint8_t*)&_easy, sizeof(STLEasyConfig)) != sizeof(STLEasyConfig)) {
    _easy = STL_EASY_DEFAULTS;
  }
  f.read((uint8_t*)_rules, sizeof(_rules));
  // V2: preberi tudi ManualBeatConfig
  if (magic == SND_MAGIC_V2) {
    if (f.read((uint8_t*)&_mbCfg, sizeof(ManualBeatConfig)) != sizeof(ManualBeatConfig)) {
      _mbCfg = MANUAL_BEAT_DEFAULTS;
    }
  } else {
    _mbCfg = MANUAL_BEAT_DEFAULTS;
  }
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
