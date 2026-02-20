#ifndef OSC_SERVER_H
#define OSC_SERVER_H

#include <WiFiUdp.h>
#include "mixer_engine.h"
#include "fixture_engine.h"

#define OSC_PORT      8000
#define OSC_BUF_SIZE  256

class OscServer {
public:
  void begin(MixerEngine* mixer, FixtureEngine* fixtures, uint16_t port = OSC_PORT);
  void update();  // Kliči iz loop()
private:
  WiFiUDP _udp;
  MixerEngine* _mixer = nullptr;
  FixtureEngine* _fixtures = nullptr;
  uint8_t _buf[OSC_BUF_SIZE];

  void dispatch(const char* addr, const char* types, const uint8_t* args, int argLen);
  float readFloat(const uint8_t* p);
  int32_t readInt32(const uint8_t* p);
  int padded(int len) { return (len + 3) & ~3; }
};

#endif
