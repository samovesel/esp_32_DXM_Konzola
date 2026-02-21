#include "link_beat.h"

#if HAS_ABLETON_LINK

// ── Full Ableton Link implementation ──

void LinkBeat::begin() {
  Serial.println("[LINK] Ableton Link knjiznica najdena");
}

void LinkBeat::enable(bool on) {
  _enabled = on;
  if (on && !_link) {
    _link = new ableton::Link(120.0);
    _link->enable(true);
    Serial.println("[LINK] Ableton Link AKTIVEN — iscem vrstnike na Wi-Fi...");
  } else if (!on && _link) {
    _link->enable(false);
    delete _link;
    _link = nullptr;
    Serial.println("[LINK] Ableton Link IZKLOPLJEN");
  }
}

void LinkBeat::update() {
  if (!_enabled || !_link) return;

  auto state = _link->captureAppSessionState();
  auto now = _link->clock().micros();

  _bpm = (float)state.tempo();
  _peers = (int)_link->numPeers();

  // Beat phase: 0.0-1.0 within current beat
  double beat = state.beatAtTime(now, 4);  // quantum = 4 beats per bar
  _prevBeatPhase = _beatPhase;
  _beatPhase = (float)(beat - floor(beat));

  // Bar phase: 0.0-1.0 within current bar
  double phase = state.phaseAtTime(now, 4);
  _barPhase = (float)(phase / 4.0);

  // Beat trigger: detect when phase wraps around
  _beatTrig = (_beatPhase < _prevBeatPhase && _prevBeatPhase > 0.5f);
}

bool LinkBeat::isConnected() const {
  return _enabled && _link && _link->numPeers() > 0;
}

float LinkBeat::getBpm() const { return _bpm; }
float LinkBeat::getBeatPhase() const { return _beatPhase; }
float LinkBeat::getBarPhase() const { return _barPhase; }
bool  LinkBeat::beatTriggered() const { return _beatTrig; }
int   LinkBeat::getPeerCount() const { return _peers; }

#else

// ── Stub implementation (library not installed) ──

void LinkBeat::begin() {
  Serial.println("[LINK] Ableton Link knjiznica NI nalozena — stub mode");
}

void LinkBeat::enable(bool on) {
  _enabled = on;
  if (on) Serial.println("[LINK] OPOZORILO: Nalozi Ableton Link knjiznico za delovanje");
}

void LinkBeat::update() {}
bool  LinkBeat::isConnected() const { return false; }
float LinkBeat::getBpm() const { return 120.0f; }
float LinkBeat::getBeatPhase() const { return 0; }
float LinkBeat::getBarPhase() const { return 0; }
bool  LinkBeat::beatTriggered() const { return false; }
int   LinkBeat::getPeerCount() const { return 0; }

#endif
