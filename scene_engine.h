#ifndef SCENE_ENGINE_H
#define SCENE_ENGINE_H

#include "config.h"

// ============================================================================
//  SceneEngine
//  Upravlja scene (shrani/recall/briši) in crossfade interpolacijo.
//  Scene so shranjene v LittleFS (/scenes/00.bin ... /scenes/19.bin).
//  Kompaktni binarni format: 24B ime + 512B DMX = 536B na sceno.
// ============================================================================

class SceneEngine {
public:
  void begin();

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

private:
  Scene* _scenes = nullptr;
  CrossfadeState _cf;

  String slotPath(int slot) const;
  bool loadSlot(int slot);
  bool saveSlot(int slot) const;
};

#endif
