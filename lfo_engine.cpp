#include "lfo_engine.h"
#include <cmath>
#include <cstring>

void LfoEngine::begin(FixtureEngine* fixtures) {
  _fixtures = fixtures;
  memset(_lfos, 0, sizeof(_lfos));
}

float LfoEngine::computeWave(uint8_t waveform, float phase) const {
  // Vrne -1.0 do 1.0
  switch ((LfoWaveform)waveform) {
    case LFO_SINE:     return sinf(phase * 2.0f * M_PI);
    case LFO_TRIANGLE: return (phase < 0.5f) ? (4.0f * phase - 1.0f) : (3.0f - 4.0f * phase);
    case LFO_SQUARE:   return (phase < 0.5f) ? 1.0f : -1.0f;
    case LFO_SAWTOOTH: return 2.0f * phase - 1.0f;
    default:           return 0;
  }
}

ChannelType LfoEngine::targetToChType(uint8_t target) const {
  switch ((LfoTarget)target) {
    case LFO_TGT_DIM:  return CH_INTENSITY;
    case LFO_TGT_PAN:  return CH_PAN;
    case LFO_TGT_TILT: return CH_TILT;
    case LFO_TGT_R:    return CH_COLOR_R;
    case LFO_TGT_G:    return CH_COLOR_G;
    case LFO_TGT_B:    return CH_COLOR_B;
    default:            return CH_INTENSITY;
  }
}

void LfoEngine::update(float dt) {
  for (int i = 0; i < MAX_LFOS; i++) {
    if (!_lfos[i].active) continue;
    _lfos[i].currentPhase += _lfos[i].rate * dt;
    if (_lfos[i].currentPhase >= 1.0f)
      _lfos[i].currentPhase -= floorf(_lfos[i].currentPhase);
  }
}

void LfoEngine::applyToOutput(const uint8_t* manualValues, uint8_t* dmxOut) {
  if (!_fixtures) return;

  for (int li = 0; li < MAX_LFOS; li++) {
    if (!_lfos[li].active) continue;
    const LfoInstance& lfo = _lfos[li];
    ChannelType chType = targetToChType(lfo.target);

    // Preštej fixture-je v maski za spread
    int fxCount = 0;
    for (int f = 0; f < MAX_FIXTURES; f++) {
      if (lfo.fixtureMask & (1UL << f)) fxCount++;
    }
    if (fxCount == 0) continue;

    int fxIdx = 0;
    for (int fi = 0; fi < MAX_FIXTURES; fi++) {
      if (!(lfo.fixtureMask & (1UL << fi))) continue;

      const PatchEntry* fx = _fixtures->getFixture(fi);
      if (!fx || !fx->active || fx->profileIndex < 0) { fxIdx++; continue; }

      // Per-fixture phase offset za spread/chase efekt
      float fiPhase = fmodf(lfo.currentPhase + lfo.phase * (float)fxIdx / (float)fxCount, 1.0f);
      float mod = computeWave(lfo.waveform, fiPhase);

      uint8_t chCount = _fixtures->fixtureChannelCount(fi);

      // Za Pan/Tilt: poišči fine kanal za 16-bit modulacijo
      int fineAddr = -1;
      if (chType == CH_PAN || chType == CH_TILT) {
        ChannelType fineType = (chType == CH_PAN) ? CH_PAN_FINE : CH_TILT_FINE;
        for (int fc = 0; fc < chCount; fc++) {
          const ChannelDef* fd = _fixtures->fixtureChannel(fi, fc);
          if (fd && fd->type == fineType) {
            int fa = fx->dmxAddress + fc;
            if (fa >= 1 && fa <= DMX_MAX_CHANNELS) fineAddr = fa - 1;
            break;
          }
        }
      }

      for (int c = 0; c < chCount; c++) {
        const ChannelDef* def = _fixtures->fixtureChannel(fi, c);
        if (!def || def->type != chType) continue;

        uint16_t addr = fx->dmxAddress + c;  // 1-based
        if (addr < 1 || addr > DMX_MAX_CHANNELS) continue;
        addr--;  // 0-based index

        if (fineAddr >= 0 && (chType == CH_PAN || chType == CH_TILT)) {
          // 16-bit modulacija: coarse+fine
          uint16_t base16 = ((uint16_t)manualValues[addr] << 8) | manualValues[fineAddr];
          float modVal16 = mod * lfo.depth * 32767.5f;
          int result16 = (int)((float)base16 + modVal16);
          if (result16 < 0) result16 = 0;
          if (result16 > 65535) result16 = 65535;
          dmxOut[addr] = (uint8_t)((result16 >> 8) & 0xFF);
          dmxOut[fineAddr] = (uint8_t)(result16 & 0xFF);
        } else {
          // 8-bit modulacija
          float base = (float)manualValues[addr];
          float modVal = mod * lfo.depth * 127.5f;
          int result = (int)(base + modVal);
          if (result < 0) result = 0;
          if (result > 255) result = 255;
          dmxOut[addr] = (uint8_t)result;
        }
      }
      fxIdx++;
    }
  }
}

int LfoEngine::addLfo(const LfoInstance& lfo) {
  for (int i = 0; i < MAX_LFOS; i++) {
    if (!_lfos[i].active) {
      _lfos[i] = lfo;
      _lfos[i].active = true;
      _lfos[i].currentPhase = 0;
      return i;
    }
  }
  return -1;
}

bool LfoEngine::removeLfo(int idx) {
  if (idx < 0 || idx >= MAX_LFOS) return false;
  _lfos[idx].active = false;
  return true;
}

bool LfoEngine::updateLfo(int idx, const LfoInstance& lfo) {
  if (idx < 0 || idx >= MAX_LFOS) return false;
  float savedPhase = _lfos[idx].currentPhase;
  _lfos[idx] = lfo;
  _lfos[idx].currentPhase = savedPhase;
  return true;
}

const LfoInstance* LfoEngine::getLfo(int idx) const {
  if (idx < 0 || idx >= MAX_LFOS) return nullptr;
  return &_lfos[idx];
}

int LfoEngine::getActiveCount() const {
  int cnt = 0;
  for (int i = 0; i < MAX_LFOS; i++) if (_lfos[i].active) cnt++;
  return cnt;
}

bool LfoEngine::isActive() const {
  for (int i = 0; i < MAX_LFOS; i++) if (_lfos[i].active) return true;
  return false;
}
