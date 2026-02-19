#ifndef FIXTURE_ENGINE_H
#define FIXTURE_ENGINE_H

#include "config.h"
#include <ArduinoJson.h>

// ============================================================================
//  FixtureEngine
//  Upravlja s profili luči, patchem in skupinami.
//  Profili se naložijo iz LittleFS (/profiles/*.json).
// ============================================================================

class FixtureEngine {
public:
  // Inicializacija — naloži profiles, patch, groups iz LittleFS
  void begin();

  // --- Profili ---
  bool loadProfile(const char* filename);       // Naloži en profil iz /profiles/
  void loadAllProfiles();                        // Naloži vse profile iz /profiles/
  int  getProfileCount() const;
  const FixtureProfile* getProfile(int idx) const;
  const FixtureProfile* findProfile(const char* id) const;
  bool deleteProfile(const char* id);

  // --- Patch ---
  bool addFixture(const char* name, const char* profileId, uint16_t dmxAddress,
                  uint8_t groupMask = 0, bool soundReactive = false);
  bool removeFixture(int index);
  bool updateFixture(int index, const PatchEntry& entry);
  void resolvePatchProfiles();                   // Poveži profileId → profileIndex
  int  getFixtureCount() const;
  const PatchEntry* getFixture(int idx) const;
  PatchEntry* getFixtureMut(int idx);
  bool savePatch();

  // --- Skupine ---
  bool setGroup(int bit, const char* name);
  bool clearGroup(int bit);
  const GroupDef* getGroup(int bit) const;
  bool saveGroups();

  // --- Pomožne ---
  // Vrne channelCount za patchan fixture (iz profila)
  uint8_t fixtureChannelCount(int fixtureIdx) const;
  // Vrne ChannelDef za fixture kanal
  const ChannelDef* fixtureChannel(int fixtureIdx, int ch) const;
  // Vrne seznam fixture indeksov v skupini
  int getFixturesInGroup(int groupBit, int* outIndices, int maxOut) const;

private:
  FixtureProfile* _profiles = nullptr;
  int _profileCount = 0;

  PatchEntry _patch[MAX_FIXTURES];

  GroupDef _groups[MAX_GROUPS];

  ChannelType parseChannelType(const char* str) const;
  void loadChannelDef(ChannelDef& ch, const JsonObject& chObj);
};

#endif
