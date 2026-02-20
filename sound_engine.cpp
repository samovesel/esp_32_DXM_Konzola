#include "sound_engine.h"
#include <arduinoFFT.h>
#include <math.h>
#include <LittleFS.h>

// Frekvenčne meje za 8 vizualizacijskih pasov
static const uint16_t BAND_EDGES[STL_BAND_COUNT + 1] = {
  30, 60, 120, 250, 500, 1000, 2000, 4000, 11000
};

// Barvne palete — 4 hue vrednosti za vsako paleto (Faza 4)
static const uint16_t PALETTE_HUES[][4] = {
  {0, 90, 180, 270},     // PAL_RAINBOW — enakomerno razporejene
  {0, 30, 45, 60},       // PAL_WARM — rdeča/oranžna/rumena
  {180, 220, 260, 300},  // PAL_COOL — modra/vijolična/cyan
  {0, 20, 40, 60},       // PAL_FIRE — rdeča/oranžna/amber/rumena
  {170, 190, 210, 240},  // PAL_OCEAN — teal/cyan/modra
  {300, 60, 180, 0},     // PAL_PARTY — magenta/rumena/cyan/rdeča
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
  memset(_mbEnvelope, 0, sizeof(_mbEnvelope));
  _agc = STL_AGC_DEFAULTS;
  memset(_bandPeaks, 0, sizeof(_bandPeaks));
  memset(_proPeaks, 0, sizeof(_proPeaks));
  memset(&_chain, 0, sizeof(_chain));
  memset(_grpState, 0, sizeof(_grpState));
  for(int g=0;g<MAX_GROUPS;g++) _grpState[g].scanDir = 1;
  _chainIdx = 0; _chainBeatCount = 0;
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

void SoundEngine::resetAgcPeaks() {
  memset(_bandPeaks, 0, sizeof(_bandPeaks));
  memset(_proPeaks, 0, sizeof(_proPeaks));
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

  // AGC decay rate iz nastavljivega parametra (0.0=počasi, 1.0=hitro)
  float decayRate = 1.0f - (0.001f + _agc.agcSpeed * 0.019f);

  // Noise gate — preveri ali je signal nad pragom šuma
  float peak = (_audio && _audio->isRunning()) ? _audio->getPeakLevel() : 0;
  float gateThresh   = _agc.noiseGate * 0.05f;
  float gateRelease  = _agc.noiseGate * 0.15f;
  bool gateOpen = (gateThresh <= 0) ||
                  (peak > gateRelease) ||
                  (_bands.bass > 0.01f && peak > gateThresh);

  for (int b = 0; b < STL_BAND_COUNT; b++) {
    int binLow  = (int)(BAND_EDGES[b] / freqPerBin);
    int binHigh = (int)(BAND_EDGES[b + 1] / freqPerBin);
    if (binLow < 1) binLow = 1;
    if (binHigh >= FFT_BINS) binHigh = FFT_BINS - 1;

    float sum = 0;
    int count = 0;
    for (int i = binLow; i <= binHigh; i++) { sum += _vReal[i]; count++; }
    float avg = count > 0 ? sum / count : 0;

    // AGC: posodobi tekoči maksimum za ta pas
    if (avg > _bandPeaks[b]) _bandPeaks[b] = avg;      // Hiter attack
    else _bandPeaks[b] *= decayRate;                     // Nastavljiv decay

    // Normaliziraj glede na tekoči maksimum z per-band gain
    float ref = fmaxf(_bandPeaks[b], AGC_MIN_FLOOR);
    float normalized = fminf(avg / ref * _easy.sensitivity * _agc.bandGains[b], 1.0f);

    // Noise gate: utišaj ko ni signala
    if (!gateOpen) normalized = 0;

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
  float decayRate = 1.0f - (0.001f + _agc.agcSpeed * 0.019f);

  // Noise gate za pro mode
  float peak = (_audio && _audio->isRunning()) ? _audio->getPeakLevel() : 0;
  float gateThresh = _agc.noiseGate * 0.05f;
  bool gateOpen = (gateThresh <= 0) || (peak > gateThresh);

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

    // AGC: posodobi tekoči maksimum za to pravilo
    if (energy > _proPeaks[r]) _proPeaks[r] = energy;
    else _proPeaks[r] *= decayRate;
    float proRef = fmaxf(_proPeaks[r], AGC_MIN_FLOOR);
    float normalized = fminf(energy / proRef * _easy.sensitivity, 1.0f);
    if (!gateOpen) normalized = 0;

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

float SoundEngine::getSubdivMultiplierFor(uint8_t subdiv) const {
  switch ((BeatSubdiv)subdiv) {
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

int SoundEngine::countSrFixturesInGroup(int groupBit) const {
  int count = 0;
  for (int i = 0; i < MAX_FIXTURES; i++) {
    const PatchEntry* fx = _fixtures->getFixture(i);
    if (fx && fx->active && fx->profileIndex >= 0 && fx->soundReactive
        && (fx->groupMask & (1 << groupBit)))
      count++;
  }
  return count;
}

int SoundEngine::getSiInGroup(int fixtureIdx, int groupBit) const {
  int idx = 0;
  for (int i = 0; i < MAX_FIXTURES; i++) {
    const PatchEntry* fx = _fixtures->getFixture(i);
    if (fx && fx->active && fx->profileIndex >= 0 && fx->soundReactive
        && (fx->groupMask & (1 << groupBit))) {
      if (i == fixtureIdx) return idx;
      idx++;
    }
  }
  return 0;
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

    // Program chain (Faza 5) — zamenjaj program ob pravem beatu
    if (_chain.active && _chain.count > 0) {
      _chainBeatCount++;
      if (_chainBeatCount >= _chain.entries[_chainIdx].durationBeats) {
        _chainBeatCount = 0;
        _chainIdx = (_chainIdx + 1) % _chain.count;
        _mbCfg.program = _chain.entries[_chainIdx].program;
        // Reset programsko-specifičnih stanj
        _mbChaseIdx = 0; _mbStackCount = 0;
        _mbScanIdx = 0; _mbScanDir = 1;
      }
    }

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

      // Random barve iz palete
      for (int i = 0; i < MAX_FIXTURES; i++) {
        _mbRandomHues[i] = paletteHue((float)random(0, 1000) / 1000.0f);
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

// Barvna paleta — interpoliraj med 4 barvami (Faza 4)
float SoundEngine::paletteHue(float t) const {
  const uint16_t* hues;
  if (_mbCfg.palette == PAL_CUSTOM) {
    hues = _mbCfg.customHues;
  } else if (_mbCfg.palette < PAL_CUSTOM) {
    hues = PALETTE_HUES[_mbCfg.palette];
  } else {
    hues = PALETTE_HUES[0];
  }
  t = fmodf(t, 1.0f);
  if (t < 0) t += 1.0f;
  float idx = t * 4.0f;
  int i0 = ((int)idx) % 4;
  int i1 = (i0 + 1) % 4;
  float frac = idx - floorf(idx);
  float h0 = (float)hues[i0];
  float h1 = (float)hues[i1];
  // Hue wrap-around
  if (fabsf(h1 - h0) > 180.0f) {
    if (h1 > h0) h0 += 360.0f; else h1 += 360.0f;
  }
  return fmodf(h0 + (h1 - h0) * frac, 360.0f);
}

void SoundEngine::applyManualBeatProgram(const uint8_t* manualValues, uint8_t* dmxOut, float dt) {
  float amount = _mbCfg.intensity;
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

  // Faza 6: Posodobi per-group beat stanja
  unsigned long now = millis();
  for (int g = 0; g < MAX_GROUPS; g++) {
    const GroupBeatOverride& ov = _mbCfg.groupOverrides[g];
    if (ov.program == GROUP_BEAT_INHERIT) {
      // Kopiraj globalno stanje
      _grpState[g].phase = _mbPhase;
      _grpState[g].smoothBeat = _mbSmoothBeat;
      continue;
    }
    // Skupina ima svoj program — izračunaj svojo fazo
    uint8_t subdiv = (ov.subdiv != GROUP_BEAT_INHERIT) ? ov.subdiv : _mbCfg.subdiv;
    float gSubMult = getSubdivMultiplierFor(subdiv);
    float gBpm = _mbCfg.bpm * gSubMult;
    float gInterval = 60000.0f / gBpm;
    if (gInterval < 100) gInterval = 100;
    float elapsed = (float)(now - _grpState[g].lastBeatMs);
    _grpState[g].phase = fminf(elapsed / gInterval, 1.0f);
    if (_grpState[g].phase >= 1.0f) {
      _grpState[g].phase = 0;
      _grpState[g].lastBeatMs = now;
      _grpState[g].beatCount++;
      _grpState[g].smoothBeat = 1.0f;
      int grpSrN = countSrFixturesInGroup(g);
      if (grpSrN > 0) {
        _grpState[g].chaseIdx = (_grpState[g].chaseIdx + 1) % grpSrN;
        _grpState[g].scanIdx += _grpState[g].scanDir;
        if (_grpState[g].scanIdx >= grpSrN - 1) { _grpState[g].scanIdx = grpSrN - 1; _grpState[g].scanDir = -1; }
        if (_grpState[g].scanIdx <= 0) { _grpState[g].scanIdx = 0; _grpState[g].scanDir = 1; }
        _grpState[g].stackCount++;
        if (_grpState[g].stackCount > grpSrN) _grpState[g].stackCount = 0;
      }
    } else {
      _grpState[g].smoothBeat = smoothValue(_grpState[g].smoothBeat, 0, 10, 300, dt);
    }
  }

  for (int si = 0; si < srN; si++) {
    int fi = srFixtures[si];
    const PatchEntry* fx = _fixtures->getFixture(fi);
    if (!fx) continue;
    uint8_t chCount = _fixtures->fixtureChannelCount(fi);

    // Faza 6: Določi efektivni program, phase, state za ta fixture
    int activeGroup = -1;
    for (int g = 0; g < MAX_GROUPS; g++) {
      if ((fx->groupMask & (1 << g)) && _mbCfg.groupOverrides[g].program != GROUP_BEAT_INHERIT) {
        const GroupDef* gd = _fixtures->getGroup(g);
        if (gd && gd->active) { activeGroup = g; break; }
      }
    }

    ManualBeatProgram prog;
    float phase, useSmoothBeat;
    int useSi, useSrN, useChaseIdx, useStackCount, useScanIdx, useBeatCount;
    float useAmount = amount;

    if (activeGroup >= 0) {
      const GroupBeatOverride& ov = _mbCfg.groupOverrides[activeGroup];
      prog = (ManualBeatProgram)ov.program;
      phase = _grpState[activeGroup].phase;
      useSmoothBeat = _grpState[activeGroup].smoothBeat;
      useSi = getSiInGroup(fi, activeGroup);
      useSrN = countSrFixturesInGroup(activeGroup);
      useChaseIdx = _grpState[activeGroup].chaseIdx;
      useStackCount = _grpState[activeGroup].stackCount;
      useScanIdx = _grpState[activeGroup].scanIdx;
      useBeatCount = _grpState[activeGroup].beatCount;
      if (ov.intensity != GROUP_BEAT_INHERIT) useAmount = (float)ov.intensity / 100.0f;
    } else {
      prog = (ManualBeatProgram)_mbCfg.program;
      phase = _mbPhase;
      useSmoothBeat = _mbSmoothBeat;
      useSi = si;
      useSrN = srN;
      useChaseIdx = _mbChaseIdx;
      useStackCount = _mbStackCount;
      useScanIdx = _mbScanIdx;
      useBeatCount = _mbBeatCount;
    }

    float dimMod = 0;
    float colorHue = -1;

    switch (prog) {
      case MBPROG_PULSE:
        dimMod = 1.0f - phase;
        break;
      case MBPROG_CHASE:
        dimMod = (useSi == useChaseIdx) ? 1.0f : 0.05f;
        break;
      case MBPROG_SINE:
        dimMod = 0.5f + 0.5f * sinf(phase * 2.0f * M_PI);
        break;
      case MBPROG_STROBE:
        dimMod = (phase < 0.15f) ? 1.0f : 0.0f;
        break;
      case MBPROG_RAINBOW: {
        dimMod = 0.6f + 0.4f * (1.0f - phase);
        float palT = fmodf(phase + (float)useSi / (float)useSrN, 1.0f);
        colorHue = paletteHue(palT);
        break;
      }
      case MBPROG_BUILD: {
        int beats = _mbCfg.buildBeats;
        if (beats < 2) beats = 8;
        int pos = useBeatCount % beats;
        float progress = (float)pos / (float)(beats - 1);
        dimMod = progress * (1.0f - phase * 0.3f);
        if (pos == beats - 1 && phase < 0.1f) dimMod = 1.0f;
        break;
      }
      case MBPROG_RANDOM:
        dimMod = 0.3f + 0.7f * useSmoothBeat;
        colorHue = _mbRandomHues[fi];
        break;
      case MBPROG_ALTERNATE: {
        bool even = (useBeatCount % 2 == 0);
        bool isEven = (useSi % 2 == 0);
        dimMod = ((even && isEven) || (!even && !isEven)) ? (1.0f - phase * 0.5f) : 0.05f;
        break;
      }
      case MBPROG_WAVE: {
        float offset = (float)useSi / (float)useSrN;
        float wavePhase = fmodf(phase + offset, 1.0f);
        dimMod = 0.5f + 0.5f * sinf(wavePhase * 2.0f * M_PI);
        break;
      }
      case MBPROG_STACK:
        dimMod = (useSi < useStackCount) ? (0.7f + 0.3f * useSmoothBeat) : 0.0f;
        break;
      case MBPROG_SPARKLE:
        if (useSmoothBeat > 0.5f) {
          dimMod = (random(0, useSrN) == useSi) ? 1.0f : 0.1f;
        } else {
          dimMod = 0.05f;
        }
        break;
      case MBPROG_SCANNER:
        dimMod = (useSi == useScanIdx) ? 1.0f : 0.05f;
        break;
      default:
        dimMod = 1.0f - phase;
        break;
    }

    // Dimmer krivulja (Faza 3)
    switch ((DimmerCurve)_mbCfg.dimCurve) {
      case DIM_EXPONENTIAL: dimMod = powf(dimMod, 2.5f); break;
      case DIM_LOGARITHMIC: dimMod = sqrtf(dimMod); break;
      default: break; // LINEAR
    }

    // Intensity envelope (Faza 3)
    if (_mbCfg.attackMs > 0 || _mbCfg.decayMs > 0) {
      float atkMs = _mbCfg.attackMs > 0 ? (float)_mbCfg.attackMs : 5.0f;
      float dcyMs = _mbCfg.decayMs  > 0 ? (float)_mbCfg.decayMs  : 50.0f;
      _mbEnvelope[fi] = smoothValue(_mbEnvelope[fi], dimMod, atkMs, dcyMs, dt);
      dimMod = _mbEnvelope[fi];
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
        float result = base + sound * useAmount;
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
#define SND_MAGIC_V2 0xAF  // V2: vključuje ManualBeatConfig (stara velikost)
#define SND_MAGIC_V3 0xB0  // V3: razširjen ManualBeatConfig + ProgramChain
#define SND_MAGIC_V4 0xB1  // V4: + STLAgcConfig (per-band gain, AGC hitrost, noise gate)
#define SND_V2_MBCFG_SIZE 20  // Velikost starega ManualBeatConfig (brez novih polj)

void SoundEngine::saveConfig() {
  File f = LittleFS.open(PATH_SOUND_CFG, "w");
  if (!f) { Serial.println("[SND] Napaka pri pisanju"); return; }
  uint8_t magic = SND_MAGIC_V4;
  f.write(&magic, 1);
  f.write((uint8_t*)&_easy, sizeof(STLEasyConfig));
  f.write((uint8_t*)_rules, sizeof(_rules));
  f.write((uint8_t*)&_mbCfg, sizeof(ManualBeatConfig));
  f.write((uint8_t*)&_chain, sizeof(ProgramChain));
  f.write((uint8_t*)&_agc, sizeof(STLAgcConfig));
  f.close();
  Serial.printf("[SND] Konfiguracija shranjena (V4, agc=%d)\n", sizeof(STLAgcConfig));
}

void SoundEngine::loadConfig() {
  File f = LittleFS.open(PATH_SOUND_CFG, "r");
  if (!f) return;
  uint8_t magic = 0;
  f.read(&magic, 1);
  if (magic != SND_MAGIC && magic != SND_MAGIC_V2 && magic != SND_MAGIC_V3 && magic != SND_MAGIC_V4) { f.close(); return; }
  if (f.read((uint8_t*)&_easy, sizeof(STLEasyConfig)) != sizeof(STLEasyConfig)) {
    _easy = STL_EASY_DEFAULTS;
  }
  f.read((uint8_t*)_rules, sizeof(_rules));
  if (magic == SND_MAGIC_V4 || magic == SND_MAGIC_V3) {
    // V3/V4: polna nova struktura
    if (f.read((uint8_t*)&_mbCfg, sizeof(ManualBeatConfig)) != sizeof(ManualBeatConfig)) {
      _mbCfg = MANUAL_BEAT_DEFAULTS;
    }
    if (f.read((uint8_t*)&_chain, sizeof(ProgramChain)) != sizeof(ProgramChain)) {
      memset(&_chain, 0, sizeof(_chain));
    }
    // V4: AGC config
    if (magic == SND_MAGIC_V4) {
      if (f.read((uint8_t*)&_agc, sizeof(STLAgcConfig)) != sizeof(STLAgcConfig)) {
        _agc = STL_AGC_DEFAULTS;
      }
    } else {
      _agc = STL_AGC_DEFAULTS;
    }
  } else if (magic == SND_MAGIC_V2) {
    // V2: stari ManualBeatConfig — preberi staro velikost, zapolni nove z defaulti
    _mbCfg = MANUAL_BEAT_DEFAULTS;
    uint8_t oldBuf[SND_V2_MBCFG_SIZE];
    if (f.read(oldBuf, SND_V2_MBCFG_SIZE) == SND_V2_MBCFG_SIZE) {
      memcpy(&_mbCfg, oldBuf, SND_V2_MBCFG_SIZE);
    }
    memset(&_chain, 0, sizeof(_chain));
    _agc = STL_AGC_DEFAULTS;
  } else {
    _mbCfg = MANUAL_BEAT_DEFAULTS;
    memset(&_chain, 0, sizeof(_chain));
    _agc = STL_AGC_DEFAULTS;
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
