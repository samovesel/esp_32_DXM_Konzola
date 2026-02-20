#ifndef SHAPE_ENGINE_H
#define SHAPE_ENGINE_H

#include "config.h"
#include "fixture_engine.h"

#define MAX_SHAPES 4

enum ShapeType : uint8_t {
  SHAPE_CIRCLE   = 0,
  SHAPE_FIGURE8  = 1,
  SHAPE_TRIANGLE = 2,
  SHAPE_SQUARE   = 3,
  SHAPE_LINE_H   = 4,
  SHAPE_LINE_V   = 5
};

struct ShapeInstance {
  bool     active;
  uint8_t  type;          // ShapeType
  float    rate;          // Hz (0.1 - 5.0)
  float    sizeX;         // Pan depth (0.0 - 1.0)
  float    sizeY;         // Tilt depth (0.0 - 1.0)
  float    phase;         // Phase spread (0.0 - 1.0)
  uint32_t fixtureMask;   // 24-bit bitmask
  float    currentPhase;  // Runtime accumulator (0.0 - 1.0)
};

class ShapeGenerator {
public:
  void begin(FixtureEngine* fixtures);
  void update(float dt);
  void applyToOutput(const uint8_t* manualValues, uint8_t* dmxOut);

  int  addShape(const ShapeInstance& shape);
  bool removeShape(int idx);
  bool updateShape(int idx, const ShapeInstance& shape);
  const ShapeInstance* getShape(int idx) const;
  int  getActiveCount() const;
  bool isActive() const;

private:
  FixtureEngine* _fixtures = nullptr;
  ShapeInstance _shapes[MAX_SHAPES];

  void computeShape(uint8_t type, float phase, float& panOut, float& tiltOut) const;
};

#endif
