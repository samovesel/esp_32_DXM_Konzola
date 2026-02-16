#include "fixture_engine.h"
#include "config_store.h"
#include <ArduinoJson.h>
#include <LittleFS.h>

void FixtureEngine::begin() {
  _profileCount = 0;
  memset(_profiles, 0, sizeof(_profiles));
  memset(_patch, 0, sizeof(_patch));
  memset(_groups, 0, sizeof(_groups));

  // Ustvari mapo za profile, če ne obstaja
  if (!LittleFS.exists(PATH_PROFILES_DIR)) {
    LittleFS.mkdir(PATH_PROFILES_DIR);
  }

  loadAllProfiles();
  patchLoad(_patch, MAX_FIXTURES);
  groupsLoad(_groups, MAX_GROUPS);
  resolvePatchProfiles();

  Serial.printf("[FIX] Naloženih %d profilov, %d fixture-ov\n", _profileCount, getFixtureCount());
}

// ============================================================================
//  PROFILI
// ============================================================================

bool FixtureEngine::loadProfile(const char* filename) {
  if (_profileCount >= MAX_PROFILES) return false;

  String path = String(PATH_PROFILES_DIR) + "/" + filename;
  File f = LittleFS.open(path, "r");
  if (!f) { Serial.printf("[FIX] Ne morem odpreti: %s\n", path.c_str()); return false; }

  JsonDocument doc;
  if (deserializeJson(doc, f)) { f.close(); return false; }
  f.close();

  // Bazno ime datoteke brez .json
  char baseId[32];
  strlcpy(baseId, filename, sizeof(baseId));
  char* dot = strrchr(baseId, '.');
  if (dot) *dot = '\0';

  const char* baseName = doc["name"] | baseId;

  // Preveri ali ima "modes" array (multi-mode profil)
  JsonArray modes = doc["modes"].as<JsonArray>();
  if (modes && modes.size() > 0) {
    // Multi-mode: ustvari en profil za vsak mode
    for (JsonObject mode : modes) {
      if (_profileCount >= MAX_PROFILES) break;
      const char* modeName = mode["name"] | "?";
      JsonArray channels = mode["channels"].as<JsonArray>();
      if (!channels || channels.size() == 0) continue;

      FixtureProfile& p = _profiles[_profileCount];
      memset(&p, 0, sizeof(p));

      // ID: "baseId__modeName" za unikatno referenco
      snprintf(p.id, sizeof(p.id), "%s__%s", baseId, modeName);
      // Ime: "BaseName (modeName)"
      snprintf(p.name, sizeof(p.name), "%s (%s)", baseName, modeName);

      p.channelCount = 0;
      for (JsonObject chObj : channels) {
        if (p.channelCount >= MAX_CHANNELS_PER_FX) break;
        loadChannelDef(p.channels[p.channelCount], chObj);
        p.channelCount++;
      }

      p.loaded = true;
      _profileCount++;
      Serial.printf("[FIX] Profil naložen: %s — %d ch\n", p.name, p.channelCount);
    }
    return true;
  }

  // Legacy single-mode profil
  FixtureProfile& p = _profiles[_profileCount];
  memset(&p, 0, sizeof(p));

  strlcpy(p.id, baseId, sizeof(p.id));
  strlcpy(p.name, baseName, sizeof(p.name));

  JsonArray channels = doc["channels"].as<JsonArray>();
  p.channelCount = 0;
  for (JsonObject chObj : channels) {
    if (p.channelCount >= MAX_CHANNELS_PER_FX) break;
    loadChannelDef(p.channels[p.channelCount], chObj);
    p.channelCount++;
  }

  p.loaded = true;
  _profileCount++;
  Serial.printf("[FIX] Profil naložen: %s (%s) — %d kanalov\n", p.name, p.id, p.channelCount);
  return true;
}

void FixtureEngine::loadChannelDef(ChannelDef& ch, const JsonObject& chObj) {
  strlcpy(ch.name, chObj["name"] | "?", sizeof(ch.name));
  ch.type = parseChannelType(chObj["type"] | "generic");
  ch.defaultValue = chObj["default"] | 0;
  ch.rangeCount = 0;

  JsonArray ranges = chObj["ranges"].as<JsonArray>();
  for (JsonObject rObj : ranges) {
    if (ch.rangeCount >= MAX_RANGES_PER_CH) break;
    ChannelRange& r = ch.ranges[ch.rangeCount];
    r.from = rObj["from"] | 0;
    r.to   = rObj["to"]   | 255;
    strlcpy(r.label, rObj["label"] | "", sizeof(r.label));
    ch.rangeCount++;
  }
}

void FixtureEngine::loadAllProfiles() {
  _profileCount = 0;
  File root = LittleFS.open(PATH_PROFILES_DIR);
  if (!root || !root.isDirectory()) return;

  File f = root.openNextFile();
  while (f) {
    String name = f.name();
    // LittleFS vrne ime brez poti, ali s potjo - odvisno od verzije
    int lastSlash = name.lastIndexOf('/');
    if (lastSlash >= 0) name = name.substring(lastSlash + 1);

    if (name.endsWith(".json")) {
      loadProfile(name.c_str());
    }
    f = root.openNextFile();
  }
}

int FixtureEngine::getProfileCount() const { return _profileCount; }

const FixtureProfile* FixtureEngine::getProfile(int idx) const {
  if (idx < 0 || idx >= _profileCount) return nullptr;
  return &_profiles[idx];
}

const FixtureProfile* FixtureEngine::findProfile(const char* id) const {
  for (int i = 0; i < _profileCount; i++) {
    if (strcmp(_profiles[i].id, id) == 0) return &_profiles[i];
  }
  return nullptr;
}

bool FixtureEngine::deleteProfile(const char* id) {
  // Multi-mode ID format: "basename__modename" → file is "basename.json"
  char fileBase[20];
  strlcpy(fileBase, id, sizeof(fileBase));
  char* sep = strstr(fileBase, "__");
  if (sep) *sep = '\0';  // Odreži mode suffix

  String path = String(PATH_PROFILES_DIR) + "/" + fileBase + ".json";
  if (!LittleFS.remove(path)) return false;

  // Ponovno naloži
  _profileCount = 0;
  loadAllProfiles();
  resolvePatchProfiles();
  return true;
}

// ============================================================================
//  PATCH
// ============================================================================

bool FixtureEngine::addFixture(const char* name, const char* profileId,
                               uint16_t dmxAddress, uint8_t groupMask, bool soundReactive) {
  for (int i = 0; i < MAX_FIXTURES; i++) {
    if (!_patch[i].active) {
      strlcpy(_patch[i].name, name, sizeof(_patch[i].name));
      strlcpy(_patch[i].profileId, profileId, sizeof(_patch[i].profileId));
      _patch[i].dmxAddress    = dmxAddress;
      _patch[i].groupMask     = groupMask;
      _patch[i].soundReactive = soundReactive;
      _patch[i].active        = true;
      _patch[i].profileIndex  = -1;

      // Poskusi povezati profil
      for (int j = 0; j < _profileCount; j++) {
        if (strcmp(_profiles[j].id, profileId) == 0) {
          _patch[i].profileIndex = j;
          break;
        }
      }
      return true;
    }
  }
  return false; // Ni prostega slota
}

bool FixtureEngine::removeFixture(int index) {
  if (index < 0 || index >= MAX_FIXTURES) return false;
  _patch[index].active = false;
  return true;
}

bool FixtureEngine::updateFixture(int index, const PatchEntry& entry) {
  if (index < 0 || index >= MAX_FIXTURES) return false;
  _patch[index] = entry;
  _patch[index].active = true;
  resolvePatchProfiles();
  return true;
}

void FixtureEngine::resolvePatchProfiles() {
  for (int i = 0; i < MAX_FIXTURES; i++) {
    if (!_patch[i].active) continue;
    _patch[i].profileIndex = -1;
    for (int j = 0; j < _profileCount; j++) {
      if (strcmp(_profiles[j].id, _patch[i].profileId) == 0) {
        _patch[i].profileIndex = j;
        break;
      }
    }
  }
}

int FixtureEngine::getFixtureCount() const {
  int count = 0;
  for (int i = 0; i < MAX_FIXTURES; i++) {
    if (_patch[i].active) count++;
  }
  return count;
}

const PatchEntry* FixtureEngine::getFixture(int idx) const {
  if (idx < 0 || idx >= MAX_FIXTURES) return nullptr;
  return &_patch[idx];
}

PatchEntry* FixtureEngine::getFixtureMut(int idx) {
  if (idx < 0 || idx >= MAX_FIXTURES) return nullptr;
  return &_patch[idx];
}

bool FixtureEngine::savePatch() {
  return patchSave(_patch, MAX_FIXTURES);
}

// ============================================================================
//  SKUPINE
// ============================================================================

bool FixtureEngine::setGroup(int bit, const char* name) {
  if (bit < 0 || bit >= MAX_GROUPS) return false;
  strlcpy(_groups[bit].name, name, sizeof(_groups[bit].name));
  _groups[bit].active = true;
  return true;
}

bool FixtureEngine::clearGroup(int bit) {
  if (bit < 0 || bit >= MAX_GROUPS) return false;
  _groups[bit].active = false;
  _groups[bit].name[0] = '\0';
  // Odstrani bitmask iz vseh fixture-ov
  for (int i = 0; i < MAX_FIXTURES; i++) {
    _patch[i].groupMask &= ~(1 << bit);
  }
  return true;
}

const GroupDef* FixtureEngine::getGroup(int bit) const {
  if (bit < 0 || bit >= MAX_GROUPS) return nullptr;
  return &_groups[bit];
}

bool FixtureEngine::saveGroups() {
  return groupsSave(_groups, MAX_GROUPS);
}

int FixtureEngine::getFixturesInGroup(int groupBit, int* outIndices, int maxOut) const {
  int count = 0;
  for (int i = 0; i < MAX_FIXTURES && count < maxOut; i++) {
    if (_patch[i].active && (_patch[i].groupMask & (1 << groupBit))) {
      outIndices[count++] = i;
    }
  }
  return count;
}

// ============================================================================
//  POMOŽNE
// ============================================================================

uint8_t FixtureEngine::fixtureChannelCount(int fixtureIdx) const {
  const PatchEntry* fx = getFixture(fixtureIdx);
  if (!fx || !fx->active || fx->profileIndex < 0) return 0;
  return _profiles[fx->profileIndex].channelCount;
}

const ChannelDef* FixtureEngine::fixtureChannel(int fixtureIdx, int ch) const {
  const PatchEntry* fx = getFixture(fixtureIdx);
  if (!fx || !fx->active || fx->profileIndex < 0) return nullptr;
  const FixtureProfile& p = _profiles[fx->profileIndex];
  if (ch < 0 || ch >= p.channelCount) return nullptr;
  return &p.channels[ch];
}

ChannelType FixtureEngine::parseChannelType(const char* str) const {
  if (!str) return CH_GENERIC;
  if (strcmp(str, "intensity") == 0) return CH_INTENSITY;
  if (strcmp(str, "color_r")   == 0) return CH_COLOR_R;
  if (strcmp(str, "color_g")   == 0) return CH_COLOR_G;
  if (strcmp(str, "color_b")   == 0) return CH_COLOR_B;
  if (strcmp(str, "color_w")   == 0) return CH_COLOR_W;
  if (strcmp(str, "color_ww")  == 0) return CH_COLOR_WW;
  if (strcmp(str, "color_a")   == 0) return CH_COLOR_A;
  if (strcmp(str, "color_uv")  == 0) return CH_COLOR_UV;
  if (strcmp(str, "color_l")   == 0) return CH_COLOR_L;
  if (strcmp(str, "color_c")   == 0) return CH_COLOR_C;
  if (strcmp(str, "pan")       == 0) return CH_PAN;
  if (strcmp(str, "pan_fine")  == 0) return CH_PAN_FINE;
  if (strcmp(str, "tilt")      == 0) return CH_TILT;
  if (strcmp(str, "tilt_fine") == 0) return CH_TILT_FINE;
  if (strcmp(str, "speed")     == 0) return CH_SPEED;
  if (strcmp(str, "gobo")      == 0) return CH_GOBO;
  if (strcmp(str, "shutter")   == 0) return CH_SHUTTER;
  if (strcmp(str, "preset")    == 0) return CH_PRESET;
  if (strcmp(str, "prism")     == 0) return CH_PRISM;
  if (strcmp(str, "focus")     == 0) return CH_FOCUS;
  if (strcmp(str, "zoom")      == 0) return CH_ZOOM;
  if (strcmp(str, "strobe")    == 0) return CH_STROBE;
  if (strcmp(str, "macro")     == 0) return CH_MACRO;
  if (strcmp(str, "cct")       == 0) return CH_CCT;
  return CH_GENERIC;
}
