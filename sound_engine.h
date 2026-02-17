#ifndef SOUND_ENGINE_H
#define SOUND_ENGINE_H

#include "config.h"
#include "audio_input.h"
#include "fixture_engine.h"

// ============================================================================
//  SoundEngine
//  FFT analiza, frekvenčni pasovi, beat detekcija, sound-to-light.
//  Podpira easy mode (preseti + cone) in pro mode (pravila).
// ============================================================================

class SoundEngine {
public:
  void begin(AudioInput* audio, FixtureEngine* fixtures);
  void update();     // Kliči vsak loop()

  // --- Easy mode ---
  STLEasyConfig& getEasyConfig() { return _easy; }
  void setEasyConfig(const STLEasyConfig& cfg) { _easy = cfg; }
  void applyPreset(uint8_t preset);

  // --- Manual beat mode ---
  ManualBeatConfig& getManualBeatConfig() { return _mbCfg; }
  void setManualBeatConfig(const ManualBeatConfig& cfg) { _mbCfg = cfg; }
  void tapBeat();                   // Tap tempo — kliči ob pritisku TAP gumba
  void setManualBpm(float bpm);     // Ročni vpis BPM
  bool isManualBeatActive() const;  // Ali manual beat poganja efekte
  float getManualBpm() const { return _mbCfg.bpm; }
  int   getTapCount() const { return _tapCount; }
  float getEffectiveBpm() const;    // Dejanski BPM (z subdivizijo)

  // --- Pro mode ---
  const STLRule* getRule(int idx) const;
  bool setRule(int idx, const STLRule& rule);
  bool clearRule(int idx);
  int  getRuleCount() const;

  // --- Rezultati ---
  const FFTBands& getBands() const { return _bands; }

  // --- Apliciranje na DMX ---
  void applyToOutput(const uint8_t* manualValues, uint8_t* dmxOut, float dt);
  bool isActive() const;

  // --- Beat sync ---
  float getBeatPhase() const { return _beatPhase; }

  // --- Persistenca ---
  void saveConfig();
  void loadConfig();

  // --- Per-fixture preview (za UI) ---
  float getFixtureLevel(int fi) const { return (fi>=0&&fi<MAX_FIXTURES)?_fxLevels[fi]:0; }

private:
  AudioInput*   _audio = nullptr;
  FixtureEngine* _fixtures = nullptr;

  // FFT
  float _vReal[FFT_SAMPLES];
  float _vImag[FFT_SAMPLES];

  // Frekvenčni pasovi
  FFTBands _bands;

  // Easy mode
  STLEasyConfig _easy;

  // Manual beat config
  ManualBeatConfig _mbCfg;

  // Pro mode
  STLRule _rules[STL_MAX_RULES];

  // Beat detection
  float _beatHistory[BEAT_HISTORY_SIZE];
  int   _beatHistIdx = 0;
  unsigned long _lastBeatTime = 0;
  float _bpmAccum = 0;
  int   _bpmCount = 0;

  // Beat sync
  float _beatPhase = 0;
  unsigned long _lastBeatMs = 0;
  float _beatIntervalMs = 500;

  // Smooth vrednosti
  float _smoothBass = 0;
  float _smoothMid = 0;
  float _smoothHigh = 0;
  float _smoothBeat = 0;

  // Per-fixture preview levels
  float _fxLevels[MAX_FIXTURES];

  // Pro mode smooth
  float _ruleValues[STL_MAX_RULES];

  // Timing
  unsigned long _lastUpdateTime = 0;

  // Rainbow HUE
  float _hueAngle = 0;

  // Manual beat state
  unsigned long _tapTimes[4];       // Časi zadnjih 4 tapov
  int           _tapCount = 0;      // Koliko tapov v trenutni seriji
  unsigned long _lastTapMs = 0;     // Čas zadnjega tapa
  float         _mbPhase = 0;       // Manual beat faza (0.0-1.0)
  unsigned long _mbLastBeatMs = 0;  // Čas zadnjega manual beata
  int           _mbBeatCount = 0;   // Skupni beat counter (za build/stack)
  float         _mbSmoothBeat = 0;  // Smooth beat za manual mode
  int           _mbChaseIdx = 0;    // Trenutni fixture v chase sekvenci
  int           _mbStackCount = 0;  // Koliko luči je prižganih (stack)
  int           _mbScanDir = 1;     // Smer skeniranja (+1/-1)
  int           _mbScanIdx = 0;     // Trenutni fixture v scanner
  float         _mbRandomHues[MAX_FIXTURES]; // Naključne barve za RANDOM program
  float         _mbTransition = 0;  // Crossfade med audio in manual (0=audio, 1=manual)
  bool          _mbAudioPresent = false; // Ali je avdio signal prisoten

  // Intensity envelope (Faza 3)
  float _mbEnvelope[MAX_FIXTURES];

  // Program chain (Faza 5)
  ProgramChain _chain;
  int   _chainIdx = 0;
  int   _chainBeatCount = 0;

  // Per-group beat state (Faza 6)
  struct GroupBeatState {
    float phase = 0;
    int   chaseIdx = 0;
    int   stackCount = 0;
    int   scanIdx = 0;
    int   scanDir = 1;
    int   beatCount = 0;
    float smoothBeat = 0;
    unsigned long lastBeatMs = 0;
  };
  GroupBeatState _grpState[MAX_GROUPS];

  void processFFT();
  void extractBands(float dt);
  void detectBeat(float dt);
  void updateBeatSync(float dt);
  void updateManualBeat(float dt);
  void applyEasyMode(const uint8_t* manualValues, uint8_t* dmxOut, float dt);
  void applyManualBeatProgram(const uint8_t* manualValues, uint8_t* dmxOut, float dt);
  void applyProMode(const uint8_t* manualValues, uint8_t* dmxOut, float dt);
  float getZoneEnergy(SoundZone zone) const;
  float applyResponseCurve(float input, ResponseCurve curve);
  float smoothValue(float current, float target, float attackMs, float decayMs, float dt);
  float getSubdivMultiplier() const;
  float getSubdivMultiplierFor(uint8_t subdiv) const;
  int   countSoundReactiveFixtures() const;
  int   countSrFixturesInGroup(int groupBit) const;
  int   getSiInGroup(int fixtureIdx, int groupBit) const;
  float paletteHue(float t) const;

public:
  // Chain access (Faza 5)
  ProgramChain& getChain() { return _chain; }
  int getChainIdx() const { return _chainIdx; }

  // Per-group phase access (Faza 6)
  float getGroupPhase(int g) const { return (g>=0&&g<MAX_GROUPS)?_grpState[g].phase:0; }
};

#endif
