#include "scene_engine.h"
#include <LittleFS.h>

void SceneEngine::begin() {
  memset(_scenes, 0, sizeof(_scenes));
  memset(&_cf, 0, sizeof(_cf));

  // Ustvari mapo za scene
  if (!LittleFS.exists(PATH_SCENES_DIR)) {
    LittleFS.mkdir(PATH_SCENES_DIR);
  }

  // Naloži vse obstoječe scene
  int loaded = 0;
  for (int i = 0; i < MAX_SCENES; i++) {
    if (loadSlot(i)) loaded++;
  }
  Serial.printf("[SCN] Naloženih %d scen\n", loaded);
}

// ============================================================================
//  DATOTEČNE OPERACIJE (binarni format)
//  Format: [24B ime][512B DMX] = 536B
// ============================================================================

String SceneEngine::slotPath(int slot) const {
  char buf[32];
  snprintf(buf, sizeof(buf), "%s/%02d.bin", PATH_SCENES_DIR, slot);
  return String(buf);
}

bool SceneEngine::loadSlot(int slot) {
  if (slot < 0 || slot >= MAX_SCENES) return false;

  File f = LittleFS.open(slotPath(slot), "r");
  if (!f) { _scenes[slot].valid = false; return false; }

  size_t expected = MAX_SCENE_NAME_LEN + DMX_MAX_CHANNELS;
  if (f.size() < expected) { f.close(); _scenes[slot].valid = false; return false; }

  f.read((uint8_t*)_scenes[slot].name, MAX_SCENE_NAME_LEN);
  _scenes[slot].name[MAX_SCENE_NAME_LEN - 1] = '\0';  // Varnostni null
  f.read(_scenes[slot].dmx, DMX_MAX_CHANNELS);
  f.close();

  _scenes[slot].valid = true;
  return true;
}

bool SceneEngine::saveSlot(int slot) const {
  if (slot < 0 || slot >= MAX_SCENES) return false;

  File f = LittleFS.open(slotPath(slot), "w");
  if (!f) return false;

  f.write((const uint8_t*)_scenes[slot].name, MAX_SCENE_NAME_LEN);
  f.write(_scenes[slot].dmx, DMX_MAX_CHANNELS);
  f.close();
  return true;
}

// ============================================================================
//  SCENE CRUD
// ============================================================================

bool SceneEngine::saveScene(int slot, const char* name, const uint8_t* dmxData) {
  if (slot < 0 || slot >= MAX_SCENES) return false;

  strlcpy(_scenes[slot].name, name, MAX_SCENE_NAME_LEN);
  memcpy(_scenes[slot].dmx, dmxData, DMX_MAX_CHANNELS);
  _scenes[slot].valid = true;

  bool ok = saveSlot(slot);
  Serial.printf("[SCN] Scena '%s' shranjena v slot %d: %s\n",
                name, slot, ok ? "OK" : "NAPAKA");
  return ok;
}

bool SceneEngine::deleteScene(int slot) {
  if (slot < 0 || slot >= MAX_SCENES) return false;

  _scenes[slot].valid = false;
  LittleFS.remove(slotPath(slot));
  Serial.printf("[SCN] Scena slot %d izbrisana\n", slot);
  return true;
}

bool SceneEngine::renameScene(int slot, const char* name) {
  if (slot < 0 || slot >= MAX_SCENES || !_scenes[slot].valid) return false;

  strlcpy(_scenes[slot].name, name, MAX_SCENE_NAME_LEN);
  return saveSlot(slot);
}

int SceneEngine::getSceneCount() const {
  int count = 0;
  for (int i = 0; i < MAX_SCENES; i++) {
    if (_scenes[i].valid) count++;
  }
  return count;
}

const Scene* SceneEngine::getScene(int slot) const {
  if (slot < 0 || slot >= MAX_SCENES) return nullptr;
  return _scenes[slot].valid ? &_scenes[slot] : nullptr;
}

int SceneEngine::findFreeSlot() const {
  for (int i = 0; i < MAX_SCENES; i++) {
    if (!_scenes[i].valid) return i;
  }
  return -1;
}

// ============================================================================
//  CROSSFADE
// ============================================================================

void SceneEngine::startCrossfade(const uint8_t* fromDmx, const uint8_t* toDmx,
                                 uint32_t durationMs, int targetSceneIdx) {
  if (durationMs == 0) {
    // Takojšen — ni crossfade-a
    _cf.active = false;
    return;
  }

  memcpy(_cf.fromDmx, fromDmx, DMX_MAX_CHANNELS);
  memcpy(_cf.toDmx, toDmx, DMX_MAX_CHANNELS);
  _cf.durationMs = durationMs;
  _cf.startTime = millis();
  _cf.targetSceneIdx = targetSceneIdx;
  _cf.active = true;

  Serial.printf("[SCN] Crossfade začet: %dms → scena %d\n", durationMs, targetSceneIdx);
}

void SceneEngine::startCrossfadeToScene(int slot, const uint8_t* currentDmx, uint32_t durationMs) {
  const Scene* sc = getScene(slot);
  if (!sc) return;

  if (durationMs == 0) {
    // Takojšen recall — brez animacije
    _cf.active = false;
    // Klicoč bo sam kopiral sc->dmx v mixer
    return;
  }

  startCrossfade(currentDmx, sc->dmx, durationMs, slot);
}

void SceneEngine::cancelCrossfade() {
  _cf.active = false;
}

float SceneEngine::getCrossfadeProgress() const {
  if (!_cf.active) return 1.0f;
  uint32_t elapsed = millis() - _cf.startTime;
  if (elapsed >= _cf.durationMs) return 1.0f;
  return (float)elapsed / (float)_cf.durationMs;
}

bool SceneEngine::updateCrossfade(uint8_t* outDmx) {
  if (!_cf.active) return false;

  uint32_t elapsed = millis() - _cf.startTime;

  if (elapsed >= _cf.durationMs) {
    // Crossfade končan — kopiraj cilj
    memcpy(outDmx, _cf.toDmx, DMX_MAX_CHANNELS);
    _cf.active = false;
    Serial.printf("[SCN] Crossfade končan (scena %d)\n", _cf.targetSceneIdx);
    return true;  // Zadnjič vrni true, da klicoč ve, da je končal
  }

  // Linearna interpolacija
  // out = from + (to - from) × progress
  // Uporabimo fiksno-vejično za hitrost: progress = elapsed × 256 / duration
  uint32_t alpha = (elapsed * 256) / _cf.durationMs;  // 0-256
  if (alpha > 256) alpha = 256;

  for (int i = 0; i < DMX_MAX_CHANNELS; i++) {
    int16_t diff = (int16_t)_cf.toDmx[i] - (int16_t)_cf.fromDmx[i];
    outDmx[i] = _cf.fromDmx[i] + (int16_t)((diff * (int32_t)alpha) / 256);
  }

  return true;
}
