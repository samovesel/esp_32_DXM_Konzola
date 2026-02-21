#ifndef SCENE_ENGINE_H
#define SCENE_ENGINE_H

#include "config.h"
#include "fixture_engine.h"

// ============================================================================
//  SceneEngine
//  Upravlja scene (shrani/recall/briši) in crossfade interpolacijo.
//  Scene so shranjene v LittleFS (/scenes/00.bin ... /scenes/19.bin).
//  Kompaktni binarni format: 24B ime + 512B DMX = 536B na sceno.
// ============================================================================

class MixerEngine;  // Forward declaration

class SceneEngine {
public:
  void begin();
  void setFixtureEngine(FixtureEngine* fix) { _fixtures = fix; }

  // --- Scene CRUD ---
  bool saveScene(int slot, const char* name, const uint8_t* dmxData);
  bool deleteScene(int slot);
  bool renameScene(int slot, const char* name);
  int  getSceneCount() const;
  const Scene* getScene(int slot) const;

  // Poišči prvi prosti slot (-1 = polno)
  int findFreeSlot() const;

  // --- Crossfade ---
  void startCrossfade(const uint8_t* fromDmx, const uint8_t* toDmx,
                      uint32_t durationMs, int targetSceneIdx = -1);
  void startCrossfadeToScene(int slot, const uint8_t* currentDmx, uint32_t durationMs);
  void cancelCrossfade();
  bool isCrossfading() const { return _cf.active; }
  float getCrossfadeProgress() const;  // 0.0 → 1.0
  int   getCrossfadeTarget() const { return _cf.targetSceneIdx; }

  // Izračunaj interpolirano stanje — kliči vsak loop()
  // Vrne true če je crossfade aktiven, in zapiše rezultat v outDmx
  bool updateCrossfade(uint8_t* outDmx);

  // --- Cue List ---
  bool loadCueList();
  bool saveCueList();
  bool addCue(int8_t sceneSlot, uint16_t fadeMs, uint16_t autoFollowMs, const char* label);
  bool removeCue(int index);
  bool updateCue(int index, int8_t sceneSlot, uint16_t fadeMs, uint16_t autoFollowMs, const char* label);
  int  getCueCount() const { return _cueCount; }
  const CueEntry* getCue(int idx) const;
  int  getCurrentCue() const { return _cueCurrent; }
  bool isCueRunning() const { return _cueRunning; }
  void cueGo(MixerEngine* mixer);
  void cueBack(MixerEngine* mixer);
  void cueGoTo(int idx, MixerEngine* mixer);
  void cueStop();
  void cueUpdateAutoFollow(MixerEngine* mixer);

private:
  Scene* _scenes = nullptr;
  FixtureEngine* _fixtures = nullptr;
  CrossfadeState _cf;

  // Cue List
  CueEntry _cues[MAX_CUES];
  uint8_t  _cueCount = 0;
  int8_t   _cueCurrent = -1;
  bool     _cueRunning = false;
  bool     _cueWaitAutoFollow = false;
  unsigned long _cueAutoFollowAt = 0;

  String slotPath(int slot) const;
  bool loadSlot(int slot);
  bool saveSlot(int slot) const;
};

#endif
