#ifndef LINK_BEAT_H
#define LINK_BEAT_H

// ============================================================================
//  Ableton Link Beat Source
//  Sinhronizira BPM in beat grid z DJ software (Traktor, Rekordbox, Ableton)
//  prek Wi-Fi UDP multicast protokola.
//
//  Zahteva: Ableton Link ESP32 knjiznico
//  GitHub: https://github.com/Ableton/link
//  ESP32 port: https://github.com/pschatzmann/esp32-esp-idf-abl-link
//
//  Namestitev: kopiraj 'include/ableton' mapo v Arduino/libraries/AbletonLink/
//  Ce knjiznica NI nalozena, modul deluje kot stub (ne dela nicesar).
// ============================================================================

#include "config.h"

// Check if Ableton Link library is available
#if __has_include(<ableton/Link.h>)
  #define HAS_ABLETON_LINK 1
  #include <ableton/Link.h>
#else
  #define HAS_ABLETON_LINK 0
#endif

class LinkBeat {
public:
  void begin();
  void update();   // Call every loop iteration

  bool isConnected() const;
  float getBpm() const;
  float getBeatPhase() const;     // 0.0-1.0 within current beat
  float getBarPhase() const;      // 0.0-1.0 within current bar (4 beats)
  bool  beatTriggered() const;    // True on beat boundary
  int   getPeerCount() const;

  void enable(bool on);
  bool isEnabled() const { return _enabled; }

private:
  bool  _enabled = false;
  float _bpm = 120.0f;
  float _beatPhase = 0;
  float _barPhase = 0;
  bool  _beatTrig = false;
  float _prevBeatPhase = 0;
  int   _peers = 0;

#if HAS_ABLETON_LINK
  ableton::Link* _link = nullptr;
#endif
};

#endif
