#include "osc_server.h"
#include <cstring>
#include <cstdlib>

void OscServer::begin(MixerEngine* mixer, FixtureEngine* fixtures, uint16_t port) {
  _mixer = mixer;
  _fixtures = fixtures;
  _udp.begin(port);
  Serial.printf("[OSC] Listening on UDP port %d\n", port);
}

void OscServer::update() {
  int pktSize = _udp.parsePacket();
  if (pktSize <= 0 || pktSize > OSC_BUF_SIZE) return;
  int len = _udp.read(_buf, OSC_BUF_SIZE);
  if (len < 8) return;

  // Parse address (null-terminated, 4-byte padded)
  const char* addr = (const char*)_buf;
  int addrLen = strnlen(addr, len);
  if (addrLen >= len) return;
  int addrPad = padded(addrLen + 1);
  if (addrPad >= len) return;

  // Parse type tag string (starts with ',')
  const char* typesRaw = (const char*)(_buf + addrPad);
  if (typesRaw[0] != ',') return;
  const char* types = typesRaw + 1;  // skip comma
  int typesRawLen = strnlen(typesRaw, len - addrPad);
  int typesPad = padded(typesRawLen + 1);

  const uint8_t* args = _buf + addrPad + typesPad;
  int argLen = len - addrPad - typesPad;
  if (argLen < 0) argLen = 0;

  _mixer->lock();
  dispatch(addr, types, args, argLen);
  _mixer->unlock();
}

float OscServer::readFloat(const uint8_t* p) {
  // OSC floats are big-endian
  uint32_t v = ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | p[3];
  float f;
  memcpy(&f, &v, 4);
  return f;
}

int32_t OscServer::readInt32(const uint8_t* p) {
  return ((int32_t)p[0] << 24) | ((int32_t)p[1] << 16) | ((int32_t)p[2] << 8) | p[3];
}

void OscServer::dispatch(const char* addr, const char* types, const uint8_t* args, int argLen) {
  // /dmx/N float(0.0-1.0) → set channel N to value * 255
  if (strncmp(addr, "/dmx/", 5) == 0 && types[0] == 'f' && argLen >= 4) {
    int ch = atoi(addr + 5);
    float val = readFloat(args);
    if (val < 0) val = 0; if (val > 1) val = 1;
    _mixer->setChannel(ch, (uint8_t)(val * 255.0f));
    return;
  }

  // /fixture/N/dimmer float → set fixture N intensity
  // /fixture/N/color  fff  → set fixture N RGB
  if (strncmp(addr, "/fixture/", 9) == 0) {
    int fi = atoi(addr + 9);
    const char* sub = strchr(addr + 9, '/');
    if (!sub) return;

    if (strcmp(sub, "/dimmer") == 0 && types[0] == 'f' && argLen >= 4) {
      float val = readFloat(args);
      if (val < 0) val = 0; if (val > 1) val = 1;
      uint8_t chCount = _fixtures->fixtureChannelCount(fi);
      for (int c = 0; c < chCount; c++) {
        const ChannelDef* def = _fixtures->fixtureChannel(fi, c);
        if (def && def->type == CH_INTENSITY)
          _mixer->setFixtureChannel(fi, c, (uint8_t)(val * 255.0f));
      }
    }
    else if (strcmp(sub, "/color") == 0 && argLen >= 12) {
      float r = readFloat(args);
      float g = readFloat(args + 4);
      float b = readFloat(args + 8);
      if (r < 0) r = 0; if (r > 1) r = 1;
      if (g < 0) g = 0; if (g > 1) g = 1;
      if (b < 0) b = 0; if (b > 1) b = 1;
      uint8_t chCount = _fixtures->fixtureChannelCount(fi);
      for (int c = 0; c < chCount; c++) {
        const ChannelDef* def = _fixtures->fixtureChannel(fi, c);
        if (!def) continue;
        if (def->type == CH_COLOR_R) _mixer->setFixtureChannel(fi, c, (uint8_t)(r * 255));
        if (def->type == CH_COLOR_G) _mixer->setFixtureChannel(fi, c, (uint8_t)(g * 255));
        if (def->type == CH_COLOR_B) _mixer->setFixtureChannel(fi, c, (uint8_t)(b * 255));
      }
    }
    return;
  }

  // /scene/N → recall scene N
  if (strncmp(addr, "/scene/", 7) == 0) {
    int slot = atoi(addr + 7);
    _mixer->recallScene(slot, CROSSFADE_DEFAULT_MS);
    return;
  }

  // /master float → set master dimmer
  if (strcmp(addr, "/master") == 0 && types[0] == 'f' && argLen >= 4) {
    float val = readFloat(args);
    if (val < 0) val = 0; if (val > 1) val = 1;
    _mixer->setMasterDimmer((uint8_t)(val * 255.0f));
    return;
  }

  // /blackout int(0/1) → blackout toggle
  if (strcmp(addr, "/blackout") == 0 && argLen >= 4) {
    int v = (types[0] == 'i') ? readInt32(args) : (int)readFloat(args);
    if (v) _mixer->blackout(); else _mixer->unBlackout();
    return;
  }
}
