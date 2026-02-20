#include "shape_engine.h"
#include <cmath>
#include <cstring>

void ShapeGenerator::begin(FixtureEngine* fixtures) {
  _fixtures = fixtures;
  memset(_shapes, 0, sizeof(_shapes));
}

void ShapeGenerator::computeShape(uint8_t type, float phase, float& panOut, float& tiltOut) const {
  float t = phase * 2.0f * M_PI;
  switch ((ShapeType)type) {
    case SHAPE_CIRCLE:
      panOut = sinf(t);
      tiltOut = cosf(t);
      break;
    case SHAPE_FIGURE8:
      panOut = sinf(t);
      tiltOut = sinf(2.0f * t);
      break;
    case SHAPE_TRIANGLE: {
      float p3 = fmodf(phase * 3.0f, 3.0f);
      if (p3 < 1.0f)      { panOut = -1.0f + 2.0f * p3;       tiltOut = -1.0f; }
      else if (p3 < 2.0f)  { panOut = 1.0f - (p3 - 1.0f);     tiltOut = -1.0f + 2.0f * (p3 - 1.0f); }
      else                  { panOut = -(p3 - 2.0f);            tiltOut = 1.0f - 2.0f * (p3 - 2.0f); }
      break;
    }
    case SHAPE_SQUARE: {
      float p4 = fmodf(phase * 4.0f, 4.0f);
      if (p4 < 1.0f)      { panOut = -1.0f + 2.0f * p4;  tiltOut = -1.0f; }
      else if (p4 < 2.0f)  { panOut = 1.0f;               tiltOut = -1.0f + 2.0f * (p4 - 1.0f); }
      else if (p4 < 3.0f)  { panOut = 1.0f - 2.0f * (p4 - 2.0f); tiltOut = 1.0f; }
      else                  { panOut = -1.0f;              tiltOut = 1.0f - 2.0f * (p4 - 3.0f); }
      break;
    }
    case SHAPE_LINE_H:
      panOut = sinf(t);
      tiltOut = 0;
      break;
    case SHAPE_LINE_V:
      panOut = 0;
      tiltOut = sinf(t);
      break;
    default:
      panOut = 0; tiltOut = 0;
  }
}

void ShapeGenerator::update(float dt) {
  for (int i = 0; i < MAX_SHAPES; i++) {
    if (!_shapes[i].active) continue;
    _shapes[i].currentPhase += _shapes[i].rate * dt;
    if (_shapes[i].currentPhase >= 1.0f)
      _shapes[i].currentPhase -= floorf(_shapes[i].currentPhase);
  }
}

void ShapeGenerator::applyToOutput(const uint8_t* manualValues, uint8_t* dmxOut) {
  if (!_fixtures) return;

  for (int si = 0; si < MAX_SHAPES; si++) {
    if (!_shapes[si].active) continue;
    const ShapeInstance& shape = _shapes[si];

    int fxCount = 0;
    for (int f = 0; f < MAX_FIXTURES; f++) {
      if (shape.fixtureMask & (1UL << f)) fxCount++;
    }
    if (fxCount == 0) continue;

    int fxIdx = 0;
    for (int fi = 0; fi < MAX_FIXTURES; fi++) {
      if (!(shape.fixtureMask & (1UL << fi))) continue;
      const PatchEntry* fx = _fixtures->getFixture(fi);
      if (!fx || !fx->active || fx->profileIndex < 0) { fxIdx++; continue; }

      float fiPhase = fmodf(shape.currentPhase + shape.phase * (float)fxIdx / (float)fxCount, 1.0f);
      float panMod, tiltMod;
      computeShape(shape.type, fiPhase, panMod, tiltMod);

      uint8_t chCount = _fixtures->fixtureChannelCount(fi);

      // Poišči Pan/Tilt coarse+fine naslove
      int panAddr = -1, panFineAddr = -1;
      int tiltAddr = -1, tiltFineAddr = -1;
      for (int c = 0; c < chCount; c++) {
        const ChannelDef* def = _fixtures->fixtureChannel(fi, c);
        if (!def) continue;
        int a = fx->dmxAddress + c - 1;  // 0-based
        if (a < 0 || a >= DMX_MAX_CHANNELS) continue;
        if (def->type == CH_PAN)       panAddr = a;
        else if (def->type == CH_PAN_FINE)  panFineAddr = a;
        else if (def->type == CH_TILT)      tiltAddr = a;
        else if (def->type == CH_TILT_FINE) tiltFineAddr = a;
      }

      // Pan modulacija
      if (panAddr >= 0) {
        if (panFineAddr >= 0) {
          uint16_t base = ((uint16_t)manualValues[panAddr] << 8) | manualValues[panFineAddr];
          int result = (int)((float)base + panMod * shape.sizeX * 32767.5f);
          if (result < 0) result = 0; if (result > 65535) result = 65535;
          dmxOut[panAddr] = (uint8_t)((result >> 8) & 0xFF);
          dmxOut[panFineAddr] = (uint8_t)(result & 0xFF);
        } else {
          float base = (float)manualValues[panAddr];
          int result = (int)(base + panMod * shape.sizeX * 127.5f);
          if (result < 0) result = 0; if (result > 255) result = 255;
          dmxOut[panAddr] = (uint8_t)result;
        }
      }

      // Tilt modulacija
      if (tiltAddr >= 0) {
        if (tiltFineAddr >= 0) {
          uint16_t base = ((uint16_t)manualValues[tiltAddr] << 8) | manualValues[tiltFineAddr];
          int result = (int)((float)base + tiltMod * shape.sizeY * 32767.5f);
          if (result < 0) result = 0; if (result > 65535) result = 65535;
          dmxOut[tiltAddr] = (uint8_t)((result >> 8) & 0xFF);
          dmxOut[tiltFineAddr] = (uint8_t)(result & 0xFF);
        } else {
          float base = (float)manualValues[tiltAddr];
          int result = (int)(base + tiltMod * shape.sizeY * 127.5f);
          if (result < 0) result = 0; if (result > 255) result = 255;
          dmxOut[tiltAddr] = (uint8_t)result;
        }
      }

      fxIdx++;
    }
  }
}

int ShapeGenerator::addShape(const ShapeInstance& shape) {
  for (int i = 0; i < MAX_SHAPES; i++) {
    if (!_shapes[i].active) {
      _shapes[i] = shape;
      _shapes[i].active = true;
      _shapes[i].currentPhase = 0;
      return i;
    }
  }
  return -1;
}

bool ShapeGenerator::removeShape(int idx) {
  if (idx < 0 || idx >= MAX_SHAPES) return false;
  _shapes[idx].active = false;
  return true;
}

bool ShapeGenerator::updateShape(int idx, const ShapeInstance& shape) {
  if (idx < 0 || idx >= MAX_SHAPES) return false;
  float savedPhase = _shapes[idx].currentPhase;
  _shapes[idx] = shape;
  _shapes[idx].currentPhase = savedPhase;
  return true;
}

const ShapeInstance* ShapeGenerator::getShape(int idx) const {
  if (idx < 0 || idx >= MAX_SHAPES) return nullptr;
  return &_shapes[idx];
}

int ShapeGenerator::getActiveCount() const {
  int cnt = 0;
  for (int i = 0; i < MAX_SHAPES; i++) if (_shapes[i].active) cnt++;
  return cnt;
}

bool ShapeGenerator::isActive() const {
  for (int i = 0; i < MAX_SHAPES; i++) if (_shapes[i].active) return true;
  return false;
}
