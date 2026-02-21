#include "scene_engine.h"
#include "mixer_engine.h"
#include <LittleFS.h>
#include <ArduinoJson.h>

void SceneEngine::begin() {
  // Alociraj scene v PSRAM (~10.7 KB)
  if (!_scenes) {
    _scenes = (Scene*)psramPreferMalloc(sizeof(Scene) * MAX_SCENES);
    if (!_scenes) { Serial.println("[SCN] NAPAKA: ne morem alocirati _scenes!"); return; }
  }
  memset(_scenes, 0, sizeof(Scene) * MAX_SCENES);
  memset(&_cf, 0, sizeof(_cf));
  memset(_cues, 0, sizeof(_cues));
  _cueCount = 0; _cueCurrent = -1; _cueRunning = false;

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
  loadCueList();
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

// Ali je tip kanala "trd" (snap, brez fade-a)?
// Gobo, Prism, Shutter, Macro, Preset kolesa ne smejo drseti — vmesne vrednosti so grde.
static bool isSnapChannel(uint8_t type) {
  return type == CH_GOBO || type == CH_SHUTTER || type == CH_PRISM ||
         type == CH_MACRO || type == CH_PRESET;
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

  // Zgradi snap masko: kateri DMX naslovi so "trdi" kanali (gobo/prism/shutter)
  // Snap kanali preskočijo na cilj na polovici crossfade-a (alpha >= 128)
  // namesto da drsijo čez vmesne vrednosti.
  bool snapMask[DMX_MAX_CHANNELS];
  memset(snapMask, 0, sizeof(snapMask));
  if (_fixtures) {
    for (int fi = 0; fi < MAX_FIXTURES; fi++) {
      const PatchEntry* fx = _fixtures->getFixture(fi);
      if (!fx || !fx->active || fx->profileIndex < 0) continue;
      uint8_t chCount = _fixtures->fixtureChannelCount(fi);
      for (int ch = 0; ch < chCount; ch++) {
        const ChannelDef* def = _fixtures->fixtureChannel(fi, ch);
        if (def && isSnapChannel(def->type)) {
          uint16_t addr = fx->dmxAddress + ch - 1;
          if (addr < DMX_MAX_CHANNELS) snapMask[addr] = true;
        }
      }
    }
  }

  for (int i = 0; i < DMX_MAX_CHANNELS; i++) {
    if (snapMask[i]) {
      // Snap: preskoči na cilj na polovici fada
      outDmx[i] = (alpha >= 128) ? _cf.toDmx[i] : _cf.fromDmx[i];
    } else {
      int16_t diff = (int16_t)_cf.toDmx[i] - (int16_t)_cf.fromDmx[i];
      outDmx[i] = _cf.fromDmx[i] + (int16_t)((diff * (int32_t)alpha) / 256);
    }
  }

  return true;
}

// ============================================================================
//  CUE LIST
// ============================================================================

bool SceneEngine::loadCueList() {
  _cueCount = 0; _cueCurrent = -1; _cueRunning = false;
  File f = LittleFS.open("/cuelist.json", "r");
  if (!f) return false;
  JsonDocument doc;
  if (deserializeJson(doc, f)) { f.close(); return false; }
  f.close();
  JsonArray arr = doc["cues"].as<JsonArray>();
  if (arr.isNull()) return false;
  for (JsonObject o : arr) {
    if (_cueCount >= MAX_CUES) break;
    CueEntry& c = _cues[_cueCount];
    c.sceneSlot = o["s"] | -1;
    c.fadeMs = o["f"] | (uint16_t)CROSSFADE_DEFAULT_MS;
    c.autoFollowMs = o["a"] | (uint16_t)0;
    strlcpy(c.label, o["l"] | "", sizeof(c.label));
    _cueCount++;
  }
  Serial.printf("[CUE] Loaded %d cues\n", _cueCount);
  return true;
}

bool SceneEngine::saveCueList() {
  JsonDocument doc;
  JsonArray arr = doc["cues"].to<JsonArray>();
  for (int i = 0; i < _cueCount; i++) {
    JsonObject o = arr.add<JsonObject>();
    o["s"] = _cues[i].sceneSlot;
    o["f"] = _cues[i].fadeMs;
    o["a"] = _cues[i].autoFollowMs;
    o["l"] = _cues[i].label;
  }
  File f = LittleFS.open("/cuelist.json", "w");
  if (!f) return false;
  serializeJson(doc, f);
  f.close();
  return true;
}

bool SceneEngine::addCue(int8_t sceneSlot, uint16_t fadeMs, uint16_t autoFollowMs, const char* label) {
  if (_cueCount >= MAX_CUES) return false;
  CueEntry& c = _cues[_cueCount];
  c.sceneSlot = sceneSlot;
  c.fadeMs = fadeMs;
  c.autoFollowMs = autoFollowMs;
  strlcpy(c.label, label ? label : "", sizeof(c.label));
  _cueCount++;
  return true;
}

bool SceneEngine::removeCue(int index) {
  if (index < 0 || index >= _cueCount) return false;
  for (int i = index; i < _cueCount - 1; i++) _cues[i] = _cues[i + 1];
  _cueCount--;
  if (_cueCurrent >= _cueCount) _cueCurrent = _cueCount - 1;
  return true;
}

bool SceneEngine::updateCue(int index, int8_t sceneSlot, uint16_t fadeMs, uint16_t autoFollowMs, const char* label) {
  if (index < 0 || index >= _cueCount) return false;
  _cues[index].sceneSlot = sceneSlot;
  _cues[index].fadeMs = fadeMs;
  _cues[index].autoFollowMs = autoFollowMs;
  if (label) strlcpy(_cues[index].label, label, sizeof(_cues[index].label));
  return true;
}

const CueEntry* SceneEngine::getCue(int idx) const {
  if (idx < 0 || idx >= _cueCount) return nullptr;
  return &_cues[idx];
}

void SceneEngine::cueGo(MixerEngine* mixer) {
  if (_cueCount == 0) return;
  int next = _cueCurrent + 1;
  if (next >= _cueCount) next = 0;
  cueGoTo(next, mixer);
}

void SceneEngine::cueBack(MixerEngine* mixer) {
  if (_cueCount == 0) return;
  int prev = _cueCurrent - 1;
  if (prev < 0) prev = _cueCount - 1;
  cueGoTo(prev, mixer);
}

void SceneEngine::cueGoTo(int idx, MixerEngine* mixer) {
  if (idx < 0 || idx >= _cueCount || !mixer) return;
  _cueCurrent = idx;
  const CueEntry& c = _cues[idx];
  if (c.sceneSlot >= 0 && c.sceneSlot < MAX_SCENES) {
    mixer->recallScene(c.sceneSlot, c.fadeMs);
  }
  if (c.autoFollowMs > 0) {
    _cueWaitAutoFollow = true;
    _cueAutoFollowAt = millis() + c.fadeMs + c.autoFollowMs;
    _cueRunning = true;
  } else {
    _cueWaitAutoFollow = false;
  }
}

void SceneEngine::cueStop() {
  _cueRunning = false;
  _cueWaitAutoFollow = false;
}

void SceneEngine::cueUpdateAutoFollow(MixerEngine* mixer) {
  if (!_cueRunning || !_cueWaitAutoFollow) return;
  if (millis() >= _cueAutoFollowAt) {
    _cueWaitAutoFollow = false;
    cueGo(mixer);
  }
}
