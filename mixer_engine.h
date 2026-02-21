#ifndef MIXER_ENGINE_H
#define MIXER_ENGINE_H

#include "config.h"
#include "fixture_engine.h"
#include "scene_engine.h"
#include <freertos/semphr.h>

class SoundEngine;      // Forward declaration
class LfoEngine;        // Forward declaration
class ShapeGenerator;   // Forward declaration

class MixerEngine {
public:
  void begin(FixtureEngine* fixtures, SceneEngine* scenes);
  void setSoundEngine(SoundEngine* sound) { _sound = sound; }
  void setLfoEngine(LfoEngine* lfo) { _lfo = lfo; }
  void setShapeGenerator(ShapeGenerator* shapes) { _shapes = shapes; }

  // --- Master Speed ---
  void setMasterSpeed(float speed);
  float getMasterSpeed() const { return _masterSpeed; }

  // --- Thread safety ---
  void lock()   { if (_mtx) xSemaphoreTake(_mtx, portMAX_DELAY); }
  void unlock() { if (_mtx) xSemaphoreGive(_mtx); }

  // --- Vhod podatkov ---
  void onArtNetData(const uint8_t* data, uint16_t length);
  void onArtPoll();

  // --- Krmilni način ---
  void switchToLocal();           // Ročni preklop na lokalno
  void switchToArtNet();          // Ročni preklop na ArtNet
  void switchToPrimaryLocal();    // Preklop na PRIMARY: ArtNet se ignorira, samo obvesti
  ControlMode getMode() const { return _mode; }
  bool isManualOverride() const { return _manualOverride; }

  // --- ArtNet timeout ---
  void setArtNetTimeout(uint32_t ms) { _artnetTimeoutMs = ms; }

  // --- ArtNet detected flag (za PRIMARY mode notifikacijo) ---
  bool consumeArtNetDetected();

  // --- Mixer operacije (delujejo samo v LOCAL načinu) ---
  void setChannel(uint16_t addr, uint8_t value);                      // Posamezen kanal (1-512)
  void setFixtureChannel(int fixtureIdx, int ch, uint8_t value);      // Fixture kanal
  void setGroupChannel(int groupBit, int ch, uint8_t value);          // Skupina: isti kanal na vseh
  void setMasterDimmer(uint8_t value);
  uint8_t getMasterDimmer() const { return _masterDimmer; }
  void setGroupDimmer(int group, uint8_t value);
  uint8_t getGroupDimmer(int group) const;
  void blackout();
  void unBlackout();
  bool isBlackout() const { return _blackout; }

  // --- Scene (delegira na SceneEngine) ---
  bool saveCurrentAsScene(int slot, const char* name);   // Shrani trenutni mixer state kot sceno
  bool recallScene(int slot, uint32_t fadeMs);            // Recall s crossfade
  bool isSceneCrossfading() const;
  float getSceneCrossfadeProgress() const;
  int   getSceneCrossfadeTarget() const;
  void  cancelSceneCrossfade();

  // --- State Snapshots ---
  void takeSnapshot(char source = 'L');  // Shrani manualValues v zgodovino
  void takeSnapshotFrom(const uint8_t* data, char source);  // Shrani specifičen buffer
  int  getSnapshotCount() const;
  const StateSnapshot* getSnapshot(int idx) const;
  void recallSnapshot(int idx);                   // Naloži stanje iz zgodovine
  void recallArtNetShadow();                      // Naloži zadnje ArtNet stanje

  // --- Undo ---
  void pushUndo();       // Shrani trenutno stanje pred spremembo
  bool undo();           // Obnovi zadnje shranjeno stanje
  bool hasUndo() const { return _undoValid; }

  // --- Locate ---
  void locateFixture(int fixtureIdx, bool on);
  bool isFixtureLocated(int fixtureIdx) const;
  uint32_t getLocateMask() const;

  // --- Izhod ---
  const uint8_t* getDmxOutput() const { return _dmxOut; }
  const uint8_t* getManualValues() const { return _manualValues; }

  // --- Persistenca ---
  void loadState();       // Naloži iz LittleFS ob zagonu
  void saveStateNow();    // Takoj shrani (za shutdown)

  // --- Loop posodobitev ---
  void update();                                  // Kliči vsak loop() — upravlja timeout

  // --- Statistika ---
  float getArtNetFps() const { return _artnetFps; }
  unsigned long getArtNetPackets() const { return _artnetPackets; }
  unsigned long getLastArtNetTime() const { return _lastArtNetPacket; }

private:
  FixtureEngine* _fixtures = nullptr;
  SceneEngine*   _scenes = nullptr;
  SoundEngine*   _sound = nullptr;
  LfoEngine*     _lfo = nullptr;
  ShapeGenerator* _shapes = nullptr;

  // Bufferji
  uint8_t _dmxOut[DMX_MAX_CHANNELS];         // Končni izhod → DMX
  uint8_t _manualValues[DMX_MAX_CHANNELS];   // Ročne (lokalne) vrednosti
  uint8_t _artnetShadow[DMX_MAX_CHANNELS];   // Senca ArtNet-a (vedno posodobljeno)

  // Stanje
  ControlMode _mode = CTRL_ARTNET;
  bool _manualOverride = false;
  bool _blackout = false;
  uint8_t _masterDimmer = 255;
  uint8_t _groupDimmers[MAX_GROUPS];

  // ArtNet timing
  uint32_t _artnetTimeoutMs = 10000;
  bool _artnetDetected = false;
  unsigned long _artnetDetectedMs = 0;
  unsigned long _lastArtNetPacket = 0;
  unsigned long _artnetPackets = 0;
  unsigned long _fpsCounter = 0;
  unsigned long _fpsLastCalc = 0;
  float _artnetFps = 0;

  // Zgodovina stanj (krožni buffer)
  StateSnapshot _snapshots[MAX_STATE_SNAPSHOTS];
  int _snapshotHead = 0;  // Naslednji slot za pisanje
  int _snapshotCount = 0;

  // Persistenca
  bool _dirty = false;
  unsigned long _dirtyTime = 0;
  unsigned long _lastSaveTime = 0;
  void markDirty();
  void checkAutoSave();
  void applyMasterDimmer();
  void applyPanTiltLimits();

  // Undo (1 korak)
  uint8_t _undoBuffer[DMX_MAX_CHANNELS];
  uint8_t _undoMaster = 255;
  bool    _undoValid = false;

  // Locate
  struct LocateState { bool active; uint8_t saved[MAX_CHANNELS_PER_FX]; };
  LocateState _locateStates[MAX_FIXTURES];

  // Thread safety
  SemaphoreHandle_t _mtx = nullptr;

  // Timing za sound dt
  unsigned long _lastUpdateMs = 0;
  float _masterSpeed = 1.0f;

  // Mode crossfade (prehod med ArtNet ↔ Local)
  bool     _modeFading = false;
  uint8_t  _modeFadeFrom[DMX_MAX_CHANNELS];  // Stanje ob preklopu
  float    _modeFadeProgress = 0;             // 0.0 → 1.0
  uint32_t _modeFadeMs = 1000;               // Trajanje v ms
  unsigned long _modeFadeStart = 0;

  void startModeFade();  // Shrani trenutni izhod kot "from"
};

#endif
