#ifndef CONFIG_STORE_H
#define CONFIG_STORE_H

#include "config.h"
#include <ArduinoJson.h>
#include <LittleFS.h>

// ============================================================================
//  NodeConfig
// ============================================================================

inline bool configLoad(NodeConfig &cfg) {
  File f = LittleFS.open(PATH_CONFIG, "r");
  if (!f) { cfg = DEFAULT_CONFIG; return false; }
  JsonDocument doc;
  if (deserializeJson(doc, f)) { f.close(); cfg = DEFAULT_CONFIG; return false; }
  f.close();

  strlcpy(cfg.hostname, doc["hostname"] | DEFAULT_CONFIG.hostname, sizeof(cfg.hostname));
  cfg.universe     = doc["universe"]     | DEFAULT_CONFIG.universe;
  cfg.channelCount = doc["channelCount"] | DEFAULT_CONFIG.channelCount;

  // WiFi APs — novo: array, staro: posamezen ssid/password
  memset(cfg.wifiAPs, 0, sizeof(cfg.wifiAPs));
  if (doc["wifiAPs"].is<JsonArray>()) {
    JsonArray arr = doc["wifiAPs"].as<JsonArray>();
    int i = 0;
    for (JsonObject ap : arr) {
      if (i >= MAX_WIFI_APS) break;
      strlcpy(cfg.wifiAPs[i].ssid, ap["ssid"] | "", sizeof(cfg.wifiAPs[i].ssid));
      strlcpy(cfg.wifiAPs[i].password, ap["password"] | "", sizeof(cfg.wifiAPs[i].password));
      i++;
    }
  } else if (doc["ssid"].is<const char*>()) {
    // Backward compat: stari format z enim ssid/password
    strlcpy(cfg.wifiAPs[0].ssid, doc["ssid"] | "", sizeof(cfg.wifiAPs[0].ssid));
    strlcpy(cfg.wifiAPs[0].password, doc["password"] | "", sizeof(cfg.wifiAPs[0].password));
  }

  cfg.dhcp = doc["dhcp"] | true;
  strlcpy(cfg.staticIp, doc["staticIp"] | DEFAULT_CONFIG.staticIp, sizeof(cfg.staticIp));
  strlcpy(cfg.staticGw, doc["staticGw"] | DEFAULT_CONFIG.staticGw, sizeof(cfg.staticGw));
  strlcpy(cfg.staticSn, doc["staticSn"] | DEFAULT_CONFIG.staticSn, sizeof(cfg.staticSn));
  cfg.audioSource = doc["audioSource"] | 0;

  // Avtentikacija
  cfg.authEnabled = doc["authEnabled"] | false;
  strlcpy(cfg.authUser, doc["authUser"] | "admin", sizeof(cfg.authUser));
  strlcpy(cfg.authPass, doc["authPass"] | "", sizeof(cfg.authPass));

  // ArtNet vedenje
  cfg.artnetTimeoutSec = doc["artnetTimeoutSec"] | (uint16_t)10;
  cfg.artnetPrimaryMode = doc["artnetPrimaryMode"] | false;
  return true;
}

inline bool configSave(const NodeConfig &cfg) {
  JsonDocument doc;
  doc["hostname"]     = cfg.hostname;
  doc["universe"]     = cfg.universe;
  doc["channelCount"] = cfg.channelCount;

  JsonArray aps = doc["wifiAPs"].to<JsonArray>();
  for (int i = 0; i < MAX_WIFI_APS; i++) {
    if (cfg.wifiAPs[i].ssid[0] == '\0') continue;
    JsonObject ap = aps.add<JsonObject>();
    ap["ssid"] = cfg.wifiAPs[i].ssid;
    ap["password"] = cfg.wifiAPs[i].password;
  }

  doc["dhcp"]         = cfg.dhcp;
  doc["staticIp"]     = cfg.staticIp;
  doc["staticGw"]     = cfg.staticGw;
  doc["staticSn"]     = cfg.staticSn;
  doc["audioSource"]  = cfg.audioSource;
  doc["authEnabled"]      = cfg.authEnabled;
  doc["authUser"]         = cfg.authUser;
  doc["authPass"]         = cfg.authPass;
  doc["artnetTimeoutSec"] = cfg.artnetTimeoutSec;
  doc["artnetPrimaryMode"]= cfg.artnetPrimaryMode;

  File f = LittleFS.open(PATH_CONFIG, "w");
  if (!f) return false;
  serializeJson(doc, f);
  f.close();
  return true;
}

// ============================================================================
//  Patch
// ============================================================================

inline bool patchLoad(PatchEntry* entries, int maxEntries) {
  for (int i = 0; i < maxEntries; i++) entries[i].active = false;

  File f = LittleFS.open(PATH_PATCH, "r");
  if (!f) return false;
  JsonDocument doc;
  if (deserializeJson(doc, f)) { f.close(); return false; }
  f.close();

  JsonArray arr = doc["fixtures"].as<JsonArray>();
  int i = 0;
  for (JsonObject obj : arr) {
    if (i >= maxEntries) break;
    strlcpy(entries[i].name,      obj["name"]      | "", sizeof(entries[i].name));
    strlcpy(entries[i].profileId, obj["profileId"] | "", sizeof(entries[i].profileId));
    entries[i].dmxAddress    = obj["dmxAddress"]    | 1;
    entries[i].groupMask     = obj["groupMask"]     | 0;
    entries[i].soundReactive = obj["soundReactive"] | false;
    entries[i].active        = true;
    entries[i].profileIndex  = -1;  // Reši se ob nalaganju profilov
    i++;
  }
  return true;
}

inline bool patchSave(const PatchEntry* entries, int maxEntries) {
  JsonDocument doc;
  JsonArray arr = doc["fixtures"].to<JsonArray>();
  for (int i = 0; i < maxEntries; i++) {
    if (!entries[i].active) continue;
    JsonObject obj = arr.add<JsonObject>();
    obj["name"]          = entries[i].name;
    obj["profileId"]     = entries[i].profileId;
    obj["dmxAddress"]    = entries[i].dmxAddress;
    obj["groupMask"]     = entries[i].groupMask;
    obj["soundReactive"] = entries[i].soundReactive;
  }
  File f = LittleFS.open(PATH_PATCH, "w");
  if (!f) return false;
  serializeJson(doc, f);
  f.close();
  return true;
}

// ============================================================================
//  Groups
// ============================================================================

inline bool groupsLoad(GroupDef* groups, int maxGroups) {
  for (int i = 0; i < maxGroups; i++) {
    groups[i].active = false;
    groups[i].name[0] = '\0';
  }

  File f = LittleFS.open(PATH_GROUPS, "r");
  if (!f) return false;
  JsonDocument doc;
  if (deserializeJson(doc, f)) { f.close(); return false; }
  f.close();

  JsonArray arr = doc["groups"].as<JsonArray>();
  int i = 0;
  for (JsonObject obj : arr) {
    if (i >= maxGroups) break;
    strlcpy(groups[i].name, obj["name"] | "", sizeof(groups[i].name));
    groups[i].active = (groups[i].name[0] != '\0');
    i++;
  }
  return true;
}

inline bool groupsSave(const GroupDef* groups, int maxGroups) {
  JsonDocument doc;
  JsonArray arr = doc["groups"].to<JsonArray>();
  for (int i = 0; i < maxGroups; i++) {
    if (!groups[i].active) continue;
    JsonObject obj = arr.add<JsonObject>();
    obj["name"] = groups[i].name;
  }
  File f = LittleFS.open(PATH_GROUPS, "w");
  if (!f) return false;
  serializeJson(doc, f);
  f.close();
  return true;
}

#endif // CONFIG_STORE_H
