#ifndef ESPNOW_DMX_H
#define ESPNOW_DMX_H

// ============================================================================
//  ESP-NOW Wireless DMX Master
//  Poslje DMX buffer brezžično na ESP-NOW slave sprejemnike.
//  Slave: poceni ESP32 + MAX485 = brezžični DMX oddajnik (3€).
//  Latenca: <1ms (peer-to-peer, brez ruterja).
//
//  Protokol:
//  - DMX buffer (512B) se fragmentira v 3 pakete po 200B (ESP-NOW limit=250B)
//  - Vsak paket: [SEQ:1][OFFSET:2][LEN:1][DATA:200]
//  - Slave sestavi celoten buffer in ga posreduje MAX485
// ============================================================================

#include "config.h"
#include <esp_now.h>

#define ESPNOW_MAX_PEERS     4
#define ESPNOW_CHUNK_SIZE  200   // Bytes per ESP-NOW packet (max 250)
#define ESPNOW_DMX_CHANNEL   1   // Wi-Fi channel (mora biti enak kot AP!)

struct EspNowPeer {
  uint8_t mac[6];
  bool    active;
  char    name[16];
  int8_t  rssi;          // Signal strength (zadnji)
};

struct EspNowConfig {
  bool    enabled = false;
  uint8_t peerCount = 0;
  EspNowPeer peers[ESPNOW_MAX_PEERS];
};

class EspNowDmx {
public:
  void begin();
  void sendFrame(const uint8_t* dmxData, uint16_t length);

  EspNowConfig& getConfig() { return _cfg; }
  bool isEnabled() const { return _cfg.enabled && _initialized; }
  int  getPeerCount() const { return _cfg.peerCount; }

  bool addPeer(const uint8_t* mac, const char* name = nullptr);
  bool removePeer(int idx);
  void setEnabled(bool on);

  void saveConfig();
  void loadConfig();

private:
  EspNowConfig _cfg;
  bool _initialized = false;
  uint8_t _seq = 0;       // Packet sequence number (wraparound)

  void initEspNow();
  void deinitEspNow();
};

#endif
