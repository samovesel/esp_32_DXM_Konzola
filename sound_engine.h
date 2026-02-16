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

  void processFFT();
  void extractBands(float dt);
  void detectBeat(float dt);
  void updateBeatSync(float dt);
  void applyEasyMode(const uint8_t* manualValues, uint8_t* dmxOut, float dt);
  void applyProMode(const uint8_t* manualValues, uint8_t* dmxOut, float dt);
  float getZoneEnergy(SoundZone zone) const;
  float applyResponseCurve(float input, ResponseCurve curve);
  float smoothValue(float current, float target, float attackMs, float decayMs, float dt);
};

#endif
