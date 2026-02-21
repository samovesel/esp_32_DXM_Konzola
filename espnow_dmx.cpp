#include "espnow_dmx.h"
#include <WiFi.h>
#include <esp_wifi.h>
#include <LittleFS.h>

// ESP-NOW send callback
static volatile int _espnowSendResult = 0;
static void onSendCb(const uint8_t* mac, esp_now_send_status_t status) {
  _espnowSendResult = (status == ESP_NOW_SEND_SUCCESS) ? 1 : -1;
}

void EspNowDmx::begin() {
  loadConfig();
  if (_cfg.enabled) initEspNow();
}

void EspNowDmx::setEnabled(bool on) {
  if (on == _cfg.enabled) return;
  _cfg.enabled = on;
  if (on) initEspNow();
  else deinitEspNow();
}

void EspNowDmx::initEspNow() {
  if (_initialized) return;

  if (esp_now_init() != ESP_OK) {
    Serial.println("[NOW] ESP-NOW init NAPAKA");
    return;
  }
  esp_now_register_send_cb(onSendCb);

  // Register all active peers
  for (int i = 0; i < _cfg.peerCount; i++) {
    if (!_cfg.peers[i].active) continue;
    esp_now_peer_info_t pi = {};
    memcpy(pi.peer_addr, _cfg.peers[i].mac, 6);
    pi.channel = 0;  // Use current Wi-Fi channel
    pi.encrypt = false;
    if (esp_now_add_peer(&pi) != ESP_OK) {
      Serial.printf("[NOW] Napaka pri dodajanju peer %d\n", i);
    }
  }

  _initialized = true;
  Serial.printf("[NOW] ESP-NOW inicializiran, %d peerov\n", _cfg.peerCount);
}

void EspNowDmx::deinitEspNow() {
  if (!_initialized) return;
  esp_now_deinit();
  _initialized = false;
  Serial.println("[NOW] ESP-NOW izklopljen");
}

void EspNowDmx::sendFrame(const uint8_t* dmxData, uint16_t length) {
  if (!_initialized || _cfg.peerCount == 0) return;
  if (length > DMX_MAX_CHANNELS) length = DMX_MAX_CHANNELS;

  _seq++;

  // Fragment DMX buffer into chunks
  // Packet format: [SEQ:1][OFFSET_HI:1][OFFSET_LO:1][LEN:1][DATA:up to 200]
  uint16_t offset = 0;
  while (offset < length) {
    uint8_t chunkLen = (length - offset > ESPNOW_CHUNK_SIZE)
                       ? ESPNOW_CHUNK_SIZE : (length - offset);

    uint8_t pkt[4 + ESPNOW_CHUNK_SIZE];
    pkt[0] = _seq;
    pkt[1] = (offset >> 8) & 0xFF;
    pkt[2] = offset & 0xFF;
    pkt[3] = chunkLen;
    memcpy(&pkt[4], &dmxData[offset], chunkLen);

    // Broadcast to all peers
    esp_now_send(NULL, pkt, 4 + chunkLen);  // NULL = all peers

    offset += chunkLen;
  }
}

bool EspNowDmx::addPeer(const uint8_t* mac, const char* name) {
  if (_cfg.peerCount >= ESPNOW_MAX_PEERS) return false;

  EspNowPeer& p = _cfg.peers[_cfg.peerCount];
  memcpy(p.mac, mac, 6);
  p.active = true;
  p.rssi = 0;
  if (name) strncpy(p.name, name, sizeof(p.name) - 1);
  else snprintf(p.name, sizeof(p.name), "Slave %d", _cfg.peerCount + 1);

  if (_initialized) {
    esp_now_peer_info_t pi = {};
    memcpy(pi.peer_addr, mac, 6);
    pi.channel = 0;
    pi.encrypt = false;
    esp_now_add_peer(&pi);
  }

  _cfg.peerCount++;
  Serial.printf("[NOW] Peer dodan: %02X:%02X:%02X:%02X:%02X:%02X\n",
                mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  return true;
}

bool EspNowDmx::removePeer(int idx) {
  if (idx < 0 || idx >= _cfg.peerCount) return false;

  if (_initialized && _cfg.peers[idx].active) {
    esp_now_del_peer(_cfg.peers[idx].mac);
  }

  // Shift remaining peers
  for (int i = idx; i < _cfg.peerCount - 1; i++) {
    _cfg.peers[i] = _cfg.peers[i + 1];
  }
  _cfg.peerCount--;
  return true;
}

void EspNowDmx::saveConfig() {
  File f = LittleFS.open("/espnow.bin", "w");
  if (!f) return;
  uint8_t ver = 1;
  f.write(&ver, 1);
  f.write((uint8_t*)&_cfg, sizeof(EspNowConfig));
  f.close();
  Serial.println("[NOW] Konfiguracija shranjena");
}

void EspNowDmx::loadConfig() {
  File f = LittleFS.open("/espnow.bin", "r");
  if (!f) return;
  uint8_t ver;
  if (f.read(&ver, 1) == 1 && ver == 1) {
    f.read((uint8_t*)&_cfg, sizeof(EspNowConfig));
  }
  f.close();
  Serial.println("[NOW] Konfiguracija nalozena");
}
