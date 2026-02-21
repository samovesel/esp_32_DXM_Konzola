#include "mixer_engine.h"
#include "sound_engine.h"
#include "lfo_engine.h"
#include "shape_engine.h"
#include <LittleFS.h>

#define MIXER_STATE_FILE   "/mixer.bin"
#define MIXER_SNAP_FILE    "/snaps.bin"
#define SAVE_DEBOUNCE_MS   3000   // Shrani 3s po zadnji spremembi
#define SAVE_MAX_WAIT_MS   10000  // Najdlje čakaj 10s

void MixerEngine::begin(FixtureEngine* fixtures, SceneEngine* scenes) {
  _fixtures = fixtures;
  _scenes = scenes;
  memset(_dmxOut, 0, sizeof(_dmxOut));
  memset(_manualValues, 0, sizeof(_manualValues));
  memset(_artnetShadow, 0, sizeof(_artnetShadow));
  memset(_snapshots, 0, sizeof(_snapshots));
  _mode = CTRL_ARTNET;
  _manualOverride = false;
  _blackout = false;
  _masterDimmer = 255;
  memset(_groupDimmers, 255, sizeof(_groupDimmers));
  _lastArtNetPacket = 0;
  _artnetPackets = 0;
  _snapshotHead = 0;
  _snapshotCount = 0;
  _dirty = false;
  _undoValid = false;
  memset(_undoBuffer, 0, sizeof(_undoBuffer));
  memset(_locateStates, 0, sizeof(_locateStates));
  _lastUpdateMs = millis();

  // Mutex za thread-safe dostop med jedroma
  _mtx = xSemaphoreCreateMutex();

  // Naloži shranjeno stanje iz LittleFS
  loadState();

  Serial.println("[MIX] Mixer inicializiran, čakam ArtNet...");
}

// ============================================================================
//  ARTNET VHOD
// ============================================================================

void MixerEngine::onArtNetData(const uint8_t* data, uint16_t length) {
  if (length > DMX_MAX_CHANNELS) length = DMX_MAX_CHANNELS;

  lock();
  // Vedno posodobi senco
  memcpy(_artnetShadow, data, length);

  _lastArtNetPacket = millis();
  _artnetPackets++;
  _fpsCounter++;

  // Če je v LOCAL_AUTO načinu in pride ArtNet → nazaj na ARTNET
  if (_mode == CTRL_LOCAL_AUTO) {
    Serial.println("[MIX] ArtNet paket zaznan → preklop nazaj na ARTNET");
    // Pravilo 3: Shrani lokalno stanje preden ArtNet prevzame
    takeSnapshot('L');
    startModeFade();
    _mode = CTRL_ARTNET;
    _manualOverride = false;
  }
  // V PRIMARY načinu: ArtNet se ignorira, samo zaznamo prisotnost za notifikacijo
  else if (_mode == CTRL_LOCAL_PRIMARY) {
    if (!_artnetDetected) {
      _artnetDetected = true;
      _artnetDetectedMs = millis();
      Serial.println("[MIX] ArtNet zaznan v PRIMARY načinu → notifikacija");
    }
  }

  // V ARTNET načinu: podatke kopira update() iz shadow
  // (onArtNetData samo posodobi shadow, update() aplicira master dimmer)
  // V LOCAL_MANUAL: ignoriramo ArtNet podatke (razen sence)
  unlock();
}

void MixerEngine::onArtPoll() {
  if (_mode == CTRL_LOCAL_AUTO) {
    Serial.println("[MIX] ArtPoll prejet v LOCAL_AUTO → preklop na ARTNET");
    takeSnapshot('L');
    startModeFade();
    _mode = CTRL_ARTNET;
    _manualOverride = false;
  }
}

void MixerEngine::startModeFade() {
  memcpy(_modeFadeFrom, _dmxOut, DMX_MAX_CHANNELS);
  _modeFadeProgress = 0;
  _modeFadeStart = millis();
  _modeFading = true;
}

// ============================================================================
//  KRMILNI NAČIN
// ============================================================================

void MixerEngine::switchToLocal() {
  if (_mode == CTRL_LOCAL_MANUAL) return;

  if (_mode == CTRL_ARTNET) {
    takeSnapshotFrom(_dmxOut, 'A');
  }

  memcpy(_manualValues, _dmxOut, DMX_MAX_CHANNELS);
  startModeFade();
  _mode = CTRL_LOCAL_MANUAL;
  _manualOverride = true;
  markDirty();
  Serial.println("[MIX] Ročni preklop na LOKALNO kontrolo");
}

void MixerEngine::switchToArtNet() {
  if (_mode == CTRL_ARTNET) return;

  takeSnapshot('L');
  startModeFade();
  _mode = CTRL_ARTNET;
  _manualOverride = false;
  _artnetDetected = false;

  memcpy(_dmxOut, _artnetShadow, DMX_MAX_CHANNELS);
  Serial.println("[MIX] Ročni preklop na ARTNET kontrolo");
}

void MixerEngine::switchToPrimaryLocal() {
  if (_mode == CTRL_LOCAL_PRIMARY) return;

  if (_mode == CTRL_ARTNET) {
    takeSnapshotFrom(_dmxOut, 'A');
    memcpy(_manualValues, _dmxOut, DMX_MAX_CHANNELS);
  }
  startModeFade();
  _mode = CTRL_LOCAL_PRIMARY;
  _manualOverride = true;
  _artnetDetected = false;
  markDirty();
  Serial.println("[MIX] Preklop na PRIMARY LOKALNO (ArtNet ignoriran, samo obvesti)");
}

bool MixerEngine::consumeArtNetDetected() {
  lock();
  bool detected = _artnetDetected;
  _artnetDetected = false;
  unlock();
  return detected;
}

// ============================================================================
//  MIXER OPERACIJE
// ============================================================================

void MixerEngine::setMasterSpeed(float speed) {
  if (speed < 0.1f) speed = 0.1f;
  if (speed > 4.0f) speed = 4.0f;
  _masterSpeed = speed;
}

void MixerEngine::setChannel(uint16_t addr, uint8_t value) {
  if (_mode == CTRL_ARTNET) return;
  if (addr < 1 || addr > DMX_MAX_CHANNELS) return;
  if (_scenes && _scenes->isCrossfading()) _scenes->cancelCrossfade();
  _manualValues[addr - 1] = value;
  markDirty();
}

void MixerEngine::setFixtureChannel(int fixtureIdx, int ch, uint8_t value) {
  if (_mode == CTRL_ARTNET || !_fixtures) return;
  const PatchEntry* fx = _fixtures->getFixture(fixtureIdx);
  if (!fx || !fx->active) return;
  uint8_t chCount = _fixtures->fixtureChannelCount(fixtureIdx);
  if (ch < 0 || ch >= chCount) return;

  // Ročna sprememba prekine crossfade
  if (_scenes && _scenes->isCrossfading()) _scenes->cancelCrossfade();

  uint16_t addr = fx->dmxAddress + ch;  // dmxAddress je 1-based
  if (addr >= 1 && addr <= DMX_MAX_CHANNELS) {
    _manualValues[addr - 1] = value;
    markDirty();
  }
}

void MixerEngine::setGroupChannel(int groupBit, int ch, uint8_t value) {
  if (_mode == CTRL_ARTNET || !_fixtures) return;
  int indices[MAX_FIXTURES];
  int count = _fixtures->getFixturesInGroup(groupBit, indices, MAX_FIXTURES);
  for (int i = 0; i < count; i++) {
    setFixtureChannel(indices[i], ch, value);
  }
}

void MixerEngine::setMasterDimmer(uint8_t value) {
  _masterDimmer = value;
  markDirty();
}

void MixerEngine::setGroupDimmer(int group, uint8_t value) {
  if (group < 0 || group >= MAX_GROUPS) return;
  _groupDimmers[group] = value;
  markDirty();
}

uint8_t MixerEngine::getGroupDimmer(int group) const {
  if (group < 0 || group >= MAX_GROUPS) return 255;
  return _groupDimmers[group];
}

void MixerEngine::blackout() {
  _blackout = true;
}

void MixerEngine::unBlackout() {
  _blackout = false;
}

// ============================================================================
//  PAN/TILT OMEJITVE IN INVERT
//  Aplicira bounding box in obračanje osi za vsak fixture
// ============================================================================

void MixerEngine::applyPanTiltLimits() {
  if (!_fixtures) return;

  for (int i = 0; i < MAX_FIXTURES; i++) {
    const PatchEntry* fx = _fixtures->getFixture(i);
    if (!fx || !fx->active || fx->profileIndex < 0) continue;

    // Hitri izhod: brez omejitev in brez invertiranja
    bool hasLimits = (fx->panMin > 0 || fx->panMax < 255 ||
                      fx->tiltMin > 0 || fx->tiltMax < 255);
    if (!fx->invertPan && !fx->invertTilt && !hasLimits) continue;

    uint8_t chCount = _fixtures->fixtureChannelCount(i);
    for (int ch = 0; ch < chCount; ch++) {
      const ChannelDef* def = _fixtures->fixtureChannel(i, ch);
      if (!def) continue;

      uint16_t addr = fx->dmxAddress + ch - 1;
      if (addr >= DMX_MAX_CHANNELS) continue;
      uint8_t val = _dmxOut[addr];

      if (def->type == CH_PAN) {
        if (fx->invertPan) val = 255 - val;
        if (fx->panMin > 0 || fx->panMax < 255) {
          // Mapiraj 0-255 v panMin-panMax
          val = fx->panMin + ((uint16_t)val * (fx->panMax - fx->panMin)) / 255;
        }
        _dmxOut[addr] = val;
      }
      else if (def->type == CH_PAN_FINE) {
        if (fx->invertPan) _dmxOut[addr] = 255 - val;
      }
      else if (def->type == CH_TILT) {
        if (fx->invertTilt) val = 255 - val;
        if (fx->tiltMin > 0 || fx->tiltMax < 255) {
          val = fx->tiltMin + ((uint16_t)val * (fx->tiltMax - fx->tiltMin)) / 255;
        }
        _dmxOut[addr] = val;
      }
      else if (def->type == CH_TILT_FINE) {
        if (fx->invertTilt) _dmxOut[addr] = 255 - val;
      }
    }
  }
}

// ============================================================================
//  MASTER DIMMER
//  Aplicira master dimmer samo na kanale tipa INTENSITY
// ============================================================================

void MixerEngine::applyMasterDimmer() {
  if (!_fixtures) return;

  // Hitri izhod če so vsi dimerji na max
  bool allMax = (_masterDimmer == 255);
  if (allMax) {
    for (int g = 0; g < MAX_GROUPS; g++) {
      if (_groupDimmers[g] < 255) { allMax = false; break; }
    }
    if (allMax) return;
  }

  for (int i = 0; i < MAX_FIXTURES; i++) {
    const PatchEntry* fx = _fixtures->getFixture(i);
    if (!fx || !fx->active || fx->profileIndex < 0) continue;

    // Najnižji group dimmer za ta fixture
    uint8_t grpDim = 255;
    for (int g = 0; g < MAX_GROUPS; g++) {
      if ((fx->groupMask & (1 << g)) && _groupDimmers[g] < grpDim) {
        grpDim = _groupDimmers[g];
      }
    }

    if (grpDim == 255 && _masterDimmer == 255) continue;

    uint8_t chCount = _fixtures->fixtureChannelCount(i);
    for (int ch = 0; ch < chCount; ch++) {
      const ChannelDef* def = _fixtures->fixtureChannel(i, ch);
      if (!def) continue;

      if (def->type == CH_INTENSITY) {
        uint16_t addr = fx->dmxAddress + ch - 1;
        if (addr < DMX_MAX_CHANNELS) {
          uint16_t val = _dmxOut[addr];
          if (grpDim < 255) val = (val * grpDim) / 255;
          if (_masterDimmer < 255) val = (val * _masterDimmer) / 255;
          _dmxOut[addr] = (uint8_t)val;
        }
      }
    }
  }
}

// ============================================================================
//  STATE SNAPSHOTS
// ============================================================================

void MixerEngine::takeSnapshot(char source) {
  StateSnapshot& s = _snapshots[_snapshotHead];
  memcpy(s.dmx, _manualValues, DMX_MAX_CHANNELS);
  s.timestamp = millis();
  s.valid = true;
  s.source = source;

  _snapshotHead = (_snapshotHead + 1) % MAX_STATE_SNAPSHOTS;
  if (_snapshotCount < MAX_STATE_SNAPSHOTS) _snapshotCount++;

  Serial.printf("[MIX] Snapshot [%c] shranjen (#%d, %d skupaj)\n", source, _snapshotHead, _snapshotCount);
  markDirty();
}

void MixerEngine::takeSnapshotFrom(const uint8_t* data, char source) {
  StateSnapshot& s = _snapshots[_snapshotHead];
  memcpy(s.dmx, data, DMX_MAX_CHANNELS);
  s.timestamp = millis();
  s.valid = true;
  s.source = source;

  _snapshotHead = (_snapshotHead + 1) % MAX_STATE_SNAPSHOTS;
  if (_snapshotCount < MAX_STATE_SNAPSHOTS) _snapshotCount++;

  Serial.printf("[MIX] Snapshot [%c] shranjen (#%d, %d skupaj)\n", source, _snapshotHead, _snapshotCount);
  markDirty();
}

int MixerEngine::getSnapshotCount() const { return _snapshotCount; }

const StateSnapshot* MixerEngine::getSnapshot(int idx) const {
  if (idx < 0 || idx >= _snapshotCount) return nullptr;
  // idx 0 = najnovejši
  int realIdx = (_snapshotHead - 1 - idx + MAX_STATE_SNAPSHOTS) % MAX_STATE_SNAPSHOTS;
  return &_snapshots[realIdx];
}

void MixerEngine::recallSnapshot(int idx) {
  const StateSnapshot* s = getSnapshot(idx);
  if (!s || !s->valid) return;
  pushUndo();
  memcpy(_manualValues, s->dmx, DMX_MAX_CHANNELS);
  // Preklopi na lokalno da vrednosti dejansko ucinokujejo
  if (_mode == CTRL_ARTNET) {
    _mode = CTRL_LOCAL_MANUAL;
    _manualOverride = true;
  }
  markDirty();
  Serial.printf("[MIX] Snapshot [%c] #%d obnovljen\n", s->source, idx);
}

void MixerEngine::recallArtNetShadow() {
  memcpy(_manualValues, _artnetShadow, DMX_MAX_CHANNELS);
  if (_mode == CTRL_ARTNET) {
    _mode = CTRL_LOCAL_MANUAL;
    _manualOverride = true;
  }
  markDirty();
  Serial.println("[MIX] ArtNet senca obnovljena v mixer");
}

// ============================================================================
//  UNDO (en korak)
// ============================================================================

void MixerEngine::pushUndo() {
  memcpy(_undoBuffer, _manualValues, DMX_MAX_CHANNELS);
  _undoMaster = _masterDimmer;
  _undoValid = true;
}

bool MixerEngine::undo() {
  if (!_undoValid) return false;
  // Swap: trenutno ↔ undo (da lahko "redo" z ponovnim undo)
  uint8_t tmp[DMX_MAX_CHANNELS];
  memcpy(tmp, _manualValues, DMX_MAX_CHANNELS);
  uint8_t tmpMaster = _masterDimmer;

  memcpy(_manualValues, _undoBuffer, DMX_MAX_CHANNELS);
  _masterDimmer = _undoMaster;

  memcpy(_undoBuffer, tmp, DMX_MAX_CHANNELS);
  _undoMaster = tmpMaster;

  if (_mode == CTRL_ARTNET) {
    _mode = CTRL_LOCAL_MANUAL;
    _manualOverride = true;
  }
  markDirty();
  Serial.println("[MIX] Undo izveden");
  return true;
}

// ============================================================================
//  LOCATE
// ============================================================================

void MixerEngine::locateFixture(int fi, bool on) {
  if (!_fixtures || fi < 0 || fi >= MAX_FIXTURES) return;
  if (_mode == CTRL_ARTNET) return;

  const PatchEntry* fx = _fixtures->getFixture(fi);
  if (!fx || !fx->active || fx->profileIndex < 0) return;

  uint8_t chCount = _fixtures->fixtureChannelCount(fi);

  if (on && !_locateStates[fi].active) {
    _locateStates[fi].active = true;
    if (_scenes && _scenes->isCrossfading()) _scenes->cancelCrossfade();

    for (int c = 0; c < chCount && c < MAX_CHANNELS_PER_FX; c++) {
      uint16_t addr = fx->dmxAddress + c;  // dmxAddress je 1-based
      if (addr < 1 || addr > DMX_MAX_CHANNELS) continue;
      _locateStates[fi].saved[c] = _manualValues[addr - 1];

      const ChannelDef* def = _fixtures->fixtureChannel(fi, c);
      if (!def) continue;

      switch (def->type) {
        case CH_INTENSITY:  _manualValues[addr - 1] = 255; break;
        case CH_COLOR_R:    _manualValues[addr - 1] = 255; break;
        case CH_COLOR_G:    _manualValues[addr - 1] = 255; break;
        case CH_COLOR_B:    _manualValues[addr - 1] = 255; break;
        case CH_COLOR_W:    _manualValues[addr - 1] = 255; break;
        case CH_PAN:        _manualValues[addr - 1] = 128; break;
        case CH_TILT:       _manualValues[addr - 1] = 128; break;
        case CH_FOCUS:      _manualValues[addr - 1] = 128; break;
        case CH_ZOOM:       _manualValues[addr - 1] = 255; break;
        case CH_GOBO:       _manualValues[addr - 1] = 0;   break;
        case CH_PRISM:      _manualValues[addr - 1] = 0;   break;
        default: break;
      }
    }
    markDirty();
  }
  else if (!on && _locateStates[fi].active) {
    for (int c = 0; c < chCount && c < MAX_CHANNELS_PER_FX; c++) {
      uint16_t addr = fx->dmxAddress + c;
      if (addr < 1 || addr > DMX_MAX_CHANNELS) continue;
      _manualValues[addr - 1] = _locateStates[fi].saved[c];
    }
    _locateStates[fi].active = false;
    markDirty();
  }
}

bool MixerEngine::isFixtureLocated(int fi) const {
  if (fi < 0 || fi >= MAX_FIXTURES) return false;
  return _locateStates[fi].active;
}

uint32_t MixerEngine::getLocateMask() const {
  uint32_t mask = 0;
  for (int i = 0; i < MAX_FIXTURES && i < 24; i++) {
    if (_locateStates[i].active) mask |= (1UL << i);
  }
  return mask;
}

// ============================================================================
//  SCENE OPERACIJE
// ============================================================================

bool MixerEngine::saveCurrentAsScene(int slot, const char* name) {
  if (!_scenes) return false;
  // Shrani trenutne manualne vrednosti (brez master dimmer efekta)
  return _scenes->saveScene(slot, name, _manualValues);
}

bool MixerEngine::recallScene(int slot, uint32_t fadeMs) {
  if (!_scenes) return false;
  if (_mode == CTRL_ARTNET) return false;  // Samo v lokalnem načinu

  const Scene* sc = _scenes->getScene(slot);
  if (!sc) return false;

  if (fadeMs == 0) {
    // Takojšen recall — brez crossfade
    pushUndo();
    memcpy(_manualValues, sc->dmx, DMX_MAX_CHANNELS);
    _scenes->cancelCrossfade();
    markDirty();
    Serial.printf("[MIX] Scena '%s' naložena (takojšen)\n", sc->name);
    return true;
  }

  // Crossfade iz trenutnega stanja v sceno
  pushUndo();
  _scenes->startCrossfade(_manualValues, sc->dmx, fadeMs, slot);
  Serial.printf("[MIX] Crossfade v sceno '%s' (%d ms)\n", sc->name, fadeMs);
  return true;
}

bool MixerEngine::isSceneCrossfading() const {
  return _scenes && _scenes->isCrossfading();
}

float MixerEngine::getSceneCrossfadeProgress() const {
  return _scenes ? _scenes->getCrossfadeProgress() : 1.0f;
}

int MixerEngine::getSceneCrossfadeTarget() const {
  return _scenes ? _scenes->getCrossfadeTarget() : -1;
}

void MixerEngine::cancelSceneCrossfade() {
  if (_scenes) _scenes->cancelCrossfade();
}

// ============================================================================
//  LOOP UPDATE
// ============================================================================

void MixerEngine::update() {
  lock();
  unsigned long now = millis();

  // --- dt za sound ---
  float dt = (now - _lastUpdateMs) / 1000.0f;
  _lastUpdateMs = now;
  if (dt <= 0 || dt > 1.0f) dt = 0.05f;
  float scaledDt = dt * _masterSpeed;  // Master Speed vpliva na efekte

  // --- FPS izračun ---
  if (now - _fpsLastCalc >= 1000) {
    _artnetFps = _fpsCounter;
    _fpsCounter = 0;
    _fpsLastCalc = now;
  }

  // --- ArtNet timeout: preklop na lokalno ---
  if (_mode == CTRL_ARTNET) {
    if (_lastArtNetPacket > 0 && (now - _lastArtNetPacket > _artnetTimeoutMs)) {
      takeSnapshotFrom(_artnetShadow, 'A');
      Serial.println("[MIX] ArtNet timeout → preklop na LOKALNO (auto)");
      memcpy(_manualValues, _artnetShadow, DMX_MAX_CHANNELS);
      markDirty();
      startModeFade();
      _mode = CTRL_LOCAL_AUTO;
    }
    else if (_lastArtNetPacket == 0 && now > _artnetTimeoutMs) {
      Serial.println("[MIX] Ni ArtNet-a → preklop na LOKALNO s shranjenim stanjem");
      startModeFade();
      _mode = CTRL_LOCAL_AUTO;
    }
  }

  // --- Sestavi izhodni buffer ---
  if (_mode == CTRL_ARTNET) {
    // FIX: Vedno kopiraj iz shadow → dmxOut pred apliciranjem masterja.
    // Brez tega se master aplicira dvakrat+ (enkrat za vsak loop brez novega ArtNet paketa).
    memcpy(_dmxOut, _artnetShadow, DMX_MAX_CHANNELS);
    applyPanTiltLimits();
    applyMasterDimmer();
  }
  else {
    // LOCAL modo: preveri crossfade, nato kopiraj v izhod
    if (_scenes && _scenes->isCrossfading()) {
      bool wasFading = _scenes->isCrossfading();  // FIX: preberi PRED update
      _scenes->updateCrossfade(_manualValues);
      if (wasFading && !_scenes->isCrossfading()) markDirty();
    }
    memcpy(_dmxOut, _manualValues, DMX_MAX_CHANNELS);

    // Sound-to-light overlay
    if (_sound) {
      _sound->applyToOutput(_manualValues, _dmxOut, scaledDt);
    }

    // LFO overlay
    if (_lfo && _lfo->isActive()) {
      _lfo->update(scaledDt);
      _lfo->applyToOutput(_manualValues, _dmxOut);
    }

    // Shape overlay
    if (_shapes && _shapes->isActive()) {
      _shapes->update(scaledDt);
      _shapes->applyToOutput(_manualValues, _dmxOut);
    }

    applyPanTiltLimits();
    applyMasterDimmer();
  }

  // --- Pametni Blackout: nulira samo Intensity + RGBW, Pan/Tilt/Gobo teče naprej ---
  if (_blackout && _fixtures) {
    for (int i = 0; i < MAX_FIXTURES; i++) {
      const PatchEntry* fx = _fixtures->getFixture(i);
      if (!fx || !fx->active || fx->profileIndex < 0) continue;
      uint8_t chCount = _fixtures->fixtureChannelCount(i);
      for (int ch = 0; ch < chCount; ch++) {
        const ChannelDef* def = _fixtures->fixtureChannel(i, ch);
        if (!def) continue;
        uint8_t t = def->type;
        if (t == CH_INTENSITY || t == CH_COLOR_R || t == CH_COLOR_G ||
            t == CH_COLOR_B || t == CH_COLOR_W || t == CH_COLOR_A ||
            t == CH_COLOR_UV || t == CH_STROBE || t == CH_COLOR_L ||
            t == CH_COLOR_C || t == CH_COLOR_WW) {
          uint16_t addr = fx->dmxAddress + ch - 1;
          if (addr < DMX_MAX_CHANNELS) _dmxOut[addr] = 0;
        }
      }
    }
  }

  // --- Mode crossfade: mehak prehod med načini ---
  if (_modeFading) {
    unsigned long elapsed = now - _modeFadeStart;
    if (elapsed >= _modeFadeMs) {
      _modeFading = false;
      _modeFadeProgress = 1.0f;
    } else {
      _modeFadeProgress = (float)elapsed / (float)_modeFadeMs;
      // Lerp: from * (1-t) + to * t
      float t = _modeFadeProgress;
      float inv = 1.0f - t;
      for (int i = 0; i < DMX_MAX_CHANNELS; i++) {
        _dmxOut[i] = (uint8_t)(inv * _modeFadeFrom[i] + t * _dmxOut[i]);
      }
    }
  }

  // Periodično shranjevanje v LittleFS (izven locka — LittleFS je počasen)
  unlock();
  checkAutoSave();
}

// ============================================================================
//  PERSISTENCA — shranjevanje/nalaganje stanja v LittleFS
// ============================================================================

void MixerEngine::markDirty() {
  if (!_dirty) {
    _dirty = true;
    _dirtyTime = millis();
  }
}

void MixerEngine::checkAutoSave() {
  if (!_dirty) return;
  unsigned long now = millis();
  unsigned long elapsed = now - _dirtyTime;

  // Shrani po 3s mirovanja ALI najdlje po 10s od prve spremembe
  if (elapsed >= SAVE_DEBOUNCE_MS ||
      (now - _lastSaveTime > SAVE_MAX_WAIT_MS && _lastSaveTime > 0)) {
    saveStateNow();
  }
}

void MixerEngine::saveStateNow() {
  // --- Mixer stanje (masterDimmer + groupDimmers + manualValues) ---
  File f = LittleFS.open(MIXER_STATE_FILE, "w");
  if (f) {
    uint8_t header[2] = { 0xAE, _masterDimmer };  // Magic V2 + master
    f.write(header, 2);
    f.write(_groupDimmers, MAX_GROUPS);
    f.write(_manualValues, DMX_MAX_CHANNELS);
    f.close();
  }

  // --- Snapshoti ---
  File sf = LittleFS.open(MIXER_SNAP_FILE, "w");
  if (sf) {
    uint8_t hdr[2] = { (uint8_t)_snapshotCount, (uint8_t)_snapshotHead };
    sf.write(hdr, 2);
    for (int i = 0; i < MAX_STATE_SNAPSHOTS; i++) {
      sf.write(_snapshots[i].dmx, DMX_MAX_CHANNELS);
      uint32_t ts = (uint32_t)_snapshots[i].timestamp;
      sf.write((uint8_t*)&ts, 4);
      uint8_t valid = _snapshots[i].valid ? 1 : 0;
      sf.write(&valid, 1);
      uint8_t src = (uint8_t)_snapshots[i].source;
      sf.write(&src, 1);
    }
    sf.close();
  }

  _dirty = false;
  _lastSaveTime = millis();
  Serial.println("[MIX] Stanje shranjeno v LittleFS");
}

void MixerEngine::loadState() {
  // --- Mixer stanje ---
  File f = LittleFS.open(MIXER_STATE_FILE, "r");
  if (f && f.size() >= (2 + DMX_MAX_CHANNELS)) {
    uint8_t header[2];
    f.read(header, 2);
    if (header[0] == 0xAE) {  // V2: z group dimmers
      _masterDimmer = header[1];
      f.read(_groupDimmers, MAX_GROUPS);
      f.read(_manualValues, DMX_MAX_CHANNELS);
      memcpy(_dmxOut, _manualValues, DMX_MAX_CHANNELS);
      Serial.printf("[MIX] Stanje V2 naloženo (master=%d)\n", _masterDimmer);
    } else if (header[0] == 0xAD) {  // V1: brez group dimmers
      _masterDimmer = header[1];
      memset(_groupDimmers, 255, sizeof(_groupDimmers));
      f.read(_manualValues, DMX_MAX_CHANNELS);
      memcpy(_dmxOut, _manualValues, DMX_MAX_CHANNELS);
      Serial.printf("[MIX] Stanje V1 naloženo (master=%d)\n", _masterDimmer);
    }
    f.close();
  } else {
    if (f) f.close();
    Serial.println("[MIX] Ni shranjega stanja — začnem s praznim");
  }

  // --- Snapshoti ---
  File sf = LittleFS.open(MIXER_SNAP_FILE, "r");
  if (sf && sf.size() >= 2) {
    uint8_t hdr[2];
    sf.read(hdr, 2);
    _snapshotCount = hdr[0];
    _snapshotHead  = hdr[1];
    if (_snapshotCount > MAX_STATE_SNAPSHOTS) _snapshotCount = MAX_STATE_SNAPSHOTS;
    if (_snapshotHead >= MAX_STATE_SNAPSHOTS) _snapshotHead = 0;

    for (int i = 0; i < MAX_STATE_SNAPSHOTS; i++) {
      sf.read(_snapshots[i].dmx, DMX_MAX_CHANNELS);
      uint32_t ts = 0;
      sf.read((uint8_t*)&ts, 4);
      _snapshots[i].timestamp = ts;
      uint8_t valid = 0;
      sf.read(&valid, 1);
      _snapshots[i].valid = (valid == 1);
      uint8_t src = 'L';
      sf.read(&src, 1);  // Stare datoteke: prebere 0 → default 'L'
      _snapshots[i].source = (src == 'A' || src == 'L') ? (char)src : 'L';
    }
    sf.close();
    Serial.printf("[MIX] Naloženih %d snapshotov\n", _snapshotCount);
  } else {
    if (sf) sf.close();
  }
}
