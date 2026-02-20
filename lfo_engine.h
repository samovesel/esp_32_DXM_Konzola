#ifndef LFO_ENGINE_H
#define LFO_ENGINE_H

#include "config.h"
#include "fixture_engine.h"

#define MAX_LFOS 8

enum LfoWaveform : uint8_t { LFO_SINE = 0, LFO_TRIANGLE = 1, LFO_SQUARE = 2, LFO_SAWTOOTH = 3 };
enum LfoTarget  : uint8_t { LFO_TGT_DIM = 0, LFO_TGT_PAN = 1, LFO_TGT_TILT = 2, LFO_TGT_R = 3, LFO_TGT_G = 4, LFO_TGT_B = 5 };

struct LfoInstance {
  bool     active;
  uint8_t  waveform;     // LfoWaveform
  uint8_t  target;       // LfoTarget
  float    rate;          // Hz (0.1 - 10.0)
  float    depth;         // 0.0 - 1.0
  float    phase;         // Phase spread (0.0 - 1.0)
  uint32_t fixtureMask;   // Bitmask za 24 fixtur
  float    currentPhase;  // Runtime accumulator (0.0 - 1.0)
};

class LfoEngine {
public:
  void begin(FixtureEngine* fixtures);
  void update(float dt);
  void applyToOutput(const uint8_t* manualValues, uint8_t* dmxOut);

  int  addLfo(const LfoInstance& lfo);
  bool removeLfo(int idx);
  bool updateLfo(int idx, const LfoInstance& lfo);
  const LfoInstance* getLfo(int idx) const;
  int  getActiveCount() const;
  bool isActive() const;

private:
  FixtureEngine* _fixtures = nullptr;
  LfoInstance _lfos[MAX_LFOS];

  float computeWave(uint8_t waveform, float phase) const;
  ChannelType targetToChType(uint8_t target) const;
};

#endif
