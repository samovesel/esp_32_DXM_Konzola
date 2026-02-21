#include "web_ui.h"
#include "config_store.h"
#include <WiFi.h>
#include <ArduinoJson.h>
#include <LittleFS.h>
#include <Update.h>
#include <esp_heap_caps.h>
#include "mbedtls/base64.h"
#include "web_ui_gz.h"

static NodeConfig*     _cfg = nullptr;
static FixtureEngine*  _fix = nullptr;
static MixerEngine*    _mix = nullptr;
static SceneEngine*    _scn = nullptr;
static SoundEngine*    _snd = nullptr;
static AudioInput*     _aud = nullptr;
static AsyncWebSocket* _ws  = nullptr;
static LfoEngine*      _lfo = nullptr;
static ShapeGenerator* _shapeGen = nullptr;
#if defined(CONFIG_IDF_TARGET_ESP32S3)
static PixelMapper*    _pxMap = nullptr;
#endif
static EspNowDmx*      _espNow = nullptr;
static unsigned long   _lastWsSend = 0;
static bool            _dmxMonActive = false;
static bool            _forceSendState = false;  // Trigger immediate full state broadcast

// HTML_PAGE_GZ in HTML_PAGE_GZ_LEN sta definirani v web_ui_gz.h

// ============================================================================
//  WebSocket handler
// ============================================================================

static void onWsEvent(AsyncWebSocket* server, AsyncWebSocketClient* client,
                      AwsEventType type, void* arg, uint8_t* data, size_t len) {
  if (type == WS_EVT_CONNECT) {
    JsonDocument hDoc;
    hDoc["t"] = "hello";
    hDoc["build"] = __DATE__ " " __TIME__;
    hDoc["fw"] = FW_VERSION;
    hDoc["flashTotal"] = LittleFS.totalBytes();
    hDoc["flashUsed"]  = LittleFS.usedBytes();
    String hJson; serializeJson(hDoc, hJson);
    client->text(hJson);
    _forceSendState = true;  // Trigger immediate full state broadcast for new client
    return;
  }
  if (type != WS_EVT_DATA) return;
  AwsFrameInfo* info = (AwsFrameInfo*)arg;
  if (!(info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT)) return;
  data[len] = 0;
  JsonDocument doc;
  if (deserializeJson(doc, (char*)data)) return;
  const char* cmd = doc["cmd"];
  if (!cmd) return;

  // Zakleni mixer za thread-safe dostop (WS teče na core 0, mixer update na core 1)
  _mix->lock();

  if (strcmp(cmd, "ch") == 0) _mix->setChannel(doc["a"]|0, doc["v"]|0);
  else if (strcmp(cmd, "fxch") == 0) _mix->setFixtureChannel(doc["f"]|0, doc["c"]|0, doc["v"]|0);
  else if (strcmp(cmd, "grch") == 0) _mix->setGroupChannel(doc["g"]|0, doc["c"]|0, doc["v"]|0);
  else if (strcmp(cmd, "hue") == 0 && _fix) {
    // HSV→RGBW+A+UV conversion on ESP32 — one message instead of 6-8
    int fi = doc["f"] | -1;
    float h = doc["h"] | 0.0f;  // Hue 0-360
    float s = doc["s"] | 1.0f;  // Saturation 0-1
    float v = doc["v"] | 1.0f;  // Value 0-1
    const PatchEntry* fx = _fix->getFixture(fi);
    if (fx && fx->active) {
      // HSV → RGB
      int hi = (int)(h / 60.0f) % 6;
      float f = h / 60.0f - (int)(h / 60.0f);
      float p = v * (1.0f - s), q = v * (1.0f - f * s), t = v * (1.0f - (1.0f - f) * s);
      float rf, gf, bf;
      switch (hi) {
        case 0: rf=v; gf=t; bf=p; break; case 1: rf=q; gf=v; bf=p; break;
        case 2: rf=p; gf=v; bf=t; break; case 3: rf=p; gf=q; bf=v; break;
        case 4: rf=t; gf=p; bf=v; break; default: rf=v; gf=p; bf=q; break;
      }
      uint8_t r=(uint8_t)(rf*255), g=(uint8_t)(gf*255), b=(uint8_t)(bf*255);
      // Scan fixture channels and set color values
      uint8_t chCount = _fix->fixtureChannelCount(fi);
      bool hasW=false, hasA=false, hasUV=false;
      for (int c=0; c<chCount; c++) {
        const ChannelDef* cd = _fix->fixtureChannel(fi, c);
        if (cd) {
          if (cd->type==CH_COLOR_W) hasW=true;
          if (cd->type==CH_COLOR_A) hasA=true;
          if (cd->type==CH_COLOR_UV) hasUV=true;
        }
      }
      uint8_t wOut = hasW ? (uint8_t)fminf(r, fminf(g, b)) : 0;
      uint8_t aOut = (hasA && r>100 && g>50 && b<100) ? (uint8_t)fminf(r, g) : 0;
      uint8_t uvOut = (hasUV && r>80 && b>150 && g<50) ? (uint8_t)fminf(r, b) : 0;
      for (int c=0; c<chCount; c++) {
        const ChannelDef* cd = _fix->fixtureChannel(fi, c);
        if (!cd) continue;
        int8_t val = -1;
        switch (cd->type) {
          case CH_COLOR_R: val=r; break; case CH_COLOR_G: val=g; break; case CH_COLOR_B: val=b; break;
          case CH_COLOR_W: val=wOut; break; case CH_COLOR_A: val=aOut; break; case CH_COLOR_UV: val=uvOut; break;
        }
        if (val >= 0) _mix->setFixtureChannel(fi, c, (uint8_t)val);
      }
    }
  }
  else if (strcmp(cmd, "master") == 0) _mix->setMasterDimmer(doc["v"]|255);
  else if (strcmp(cmd, "grpdim") == 0) _mix->setGroupDimmer(doc["g"]|0, doc["v"]|255);
  else if (strcmp(cmd, "blackout") == 0) { if(doc["v"]|0) _mix->blackout(); else _mix->unBlackout(); }
  else if (strcmp(cmd, "flash") == 0) _mix->setFlash((doc["v"]|0)!=0, doc["l"]|255);
  else if (strcmp(cmd, "mode") == 0) { const char* m=doc["v"]; if(m&&strcmp(m,"local")==0) _mix->switchToLocal(); else if(m&&strcmp(m,"artnet")==0) _mix->switchToArtNet(); else if(m&&strcmp(m,"primary_local")==0) _mix->switchToPrimaryLocal(); }
  else if (strcmp(cmd, "switchToArtnet") == 0) _mix->switchToArtNet();
  else if (strcmp(cmd, "recall") == 0) _mix->recallSnapshot(doc["i"]|0);
  else if (strcmp(cmd, "recall_artnet") == 0) _mix->recallArtNetShadow();
  else if (strcmp(cmd, "scene_recall") == 0) _mix->recallScene(doc["slot"]|-1, doc["fade"]|CROSSFADE_DEFAULT_MS);
  else if (strcmp(cmd, "undo") == 0) _mix->undo();
  else if (strcmp(cmd, "locate") == 0) _mix->locateFixture(doc["f"]|0, (doc["on"]|0)!=0);
  else if (strcmp(cmd, "dmxmon") == 0) _dmxMonActive = (doc["on"]|0) != 0;
  else if (strcmp(cmd, "cue_go") == 0 && _scn) _scn->cueGo(_mix);
  else if (strcmp(cmd, "cue_back") == 0 && _scn) _scn->cueBack(_mix);
  else if (strcmp(cmd, "cue_goto") == 0 && _scn) _scn->cueGoTo(doc["i"]|0, _mix);
  else if (strcmp(cmd, "cue_stop") == 0 && _scn) _scn->cueStop();
  else if (strcmp(cmd, "cue_add") == 0 && _scn) { _scn->addCue(doc["s"]|-1, doc["f"]|1500, doc["a"]|0, doc["l"]|""); _scn->saveCueList(); }
  else if (strcmp(cmd, "cue_rm") == 0 && _scn) { _scn->removeCue(doc["i"]|0); _scn->saveCueList(); }
  else if (strcmp(cmd, "cue_upd") == 0 && _scn) { _scn->updateCue(doc["i"]|0, doc["s"]|-1, doc["f"]|1500, doc["a"]|0, doc["l"]|""); _scn->saveCueList(); }
  else if (strcmp(cmd, "lfo_add") == 0 && _lfo) {
    LfoInstance l = {}; l.active = true;
    l.waveform = doc["w"] | 0; l.target = doc["tgt"] | 0;
    l.rate = doc["rate"] | 1.0f; l.depth = doc["depth"] | 0.5f;
    l.phase = doc["phase"] | 0.0f; l.fixtureMask = doc["mask"] | 0UL;
    l.symmetry = doc["sym"] | 0;
    _lfo->addLfo(l);
  }
  else if (strcmp(cmd, "lfo_rm") == 0 && _lfo) { _lfo->removeLfo(doc["i"]|0); }
  else if (strcmp(cmd, "lfo_upd") == 0 && _lfo) {
    int idx = doc["i"] | -1;
    const LfoInstance* existing = _lfo->getLfo(idx);
    if (existing) {
      LfoInstance l = *existing;
      if (!doc["w"].isNull()) l.waveform = doc["w"] | 0;
      if (!doc["tgt"].isNull()) l.target = doc["tgt"] | 0;
      if (!doc["rate"].isNull()) l.rate = doc["rate"] | 1.0f;
      if (!doc["depth"].isNull()) l.depth = doc["depth"] | 0.5f;
      if (!doc["phase"].isNull()) l.phase = doc["phase"] | 0.0f;
      if (!doc["mask"].isNull()) l.fixtureMask = doc["mask"] | 0UL;
      if (!doc["sym"].isNull()) l.symmetry = doc["sym"] | 0;
      _lfo->updateLfo(idx, l);
    }
  }
  else if (strcmp(cmd, "lfo_clear") == 0 && _lfo) {
    for (int i = 0; i < MAX_LFOS; i++) _lfo->removeLfo(i);
  }
  else if (strcmp(cmd, "master_speed") == 0) {
    _mix->setMasterSpeed(doc["v"] | 1.0f);
  }
  else if (strcmp(cmd, "shape_add") == 0 && _shapeGen) {
    ShapeInstance s = {}; s.active = true;
    s.type = doc["type"] | 0;
    s.rate = doc["rate"] | 0.5f;
    s.sizeX = doc["sx"] | 0.5f;
    s.sizeY = doc["sy"] | 0.5f;
    s.phase = doc["phase"] | 0.0f;
    s.fixtureMask = doc["mask"] | 0UL;
    _shapeGen->addShape(s);
  }
  else if (strcmp(cmd, "shape_rm") == 0 && _shapeGen) {
    _shapeGen->removeShape(doc["i"] | 0);
  }
  else if (strcmp(cmd, "shape_upd") == 0 && _shapeGen) {
    int idx = doc["i"] | -1;
    const ShapeInstance* existing = _shapeGen->getShape(idx);
    if (existing) {
      ShapeInstance s = *existing;
      if (!doc["type"].isNull()) s.type = doc["type"] | 0;
      if (!doc["rate"].isNull()) s.rate = doc["rate"] | 0.5f;
      if (!doc["sx"].isNull()) s.sizeX = doc["sx"] | 0.5f;
      if (!doc["sy"].isNull()) s.sizeY = doc["sy"] | 0.5f;
      if (!doc["phase"].isNull()) s.phase = doc["phase"] | 0.0f;
      if (!doc["mask"].isNull()) s.fixtureMask = doc["mask"] | 0UL;
      _shapeGen->updateShape(idx, s);
    }
  }
  else if (strcmp(cmd, "shape_clear") == 0 && _shapeGen) {
    for (int i = 0; i < MAX_SHAPES; i++) _shapeGen->removeShape(i);
  }
  else if (strcmp(cmd, "easy") == 0 && _snd) {
    STLEasyConfig& e = _snd->getEasyConfig();
    e.enabled       = (doc["en"]|0) != 0;
    e.sensitivity   = doc["sens"] | 1.0f;
    e.soundAmount   = doc["amt"]  | 0.5f;
    e.bassIntensity = (doc["bass"]|1) != 0;
    e.midColor      = (doc["mid"]|1)  != 0;
    e.highStrobe    = (doc["high"]|1) != 0;
    e.beatBump      = (doc["beat"]|1) != 0;
    e.beatSync      = (doc["bsync"]|0) != 0;
    if (!doc["preset"].isNull()) {
      uint8_t p = doc["preset"]|0;
      if (p != SPRESET_CUSTOM) _snd->applyPreset(p);
      else e.preset = SPRESET_CUSTOM;
    }
  }
  else if (strcmp(cmd, "zone") == 0 && _snd) {
    int fi = doc["fi"]|-1;
    int z  = doc["z"]|0;
    if (fi >= 0 && fi < MAX_FIXTURES) _snd->getEasyConfig().zones[fi] = (uint8_t)z;
  }
  else if (strcmp(cmd, "agc") == 0 && _snd) {
    STLAgcConfig& agc = _snd->getAgcConfig();
    if (!doc["spd"].isNull()) agc.agcSpeed  = doc["spd"] | 0.5f;
    if (!doc["ng"].isNull())  agc.noiseGate = doc["ng"]  | 0.3f;
    JsonArray bg = doc["bg"].as<JsonArray>();
    if (bg) {
      int i = 0;
      for (JsonVariant v : bg) {
        if (i >= STL_BAND_COUNT) break;
        agc.bandGains[i] = v | 1.0f;
        i++;
      }
    }
    _snd->resetAgcPeaks();
  }
  else if (strcmp(cmd, "save_sound") == 0 && _snd) {
    _snd->saveConfig();
  }
  // Manual beat commands
  else if (strcmp(cmd, "tap") == 0 && _snd) {
    _snd->tapBeat();
  }
  else if (strcmp(cmd, "mb_bpm") == 0 && _snd) {
    _snd->setManualBpm(doc["v"] | 120.0f);
  }
  else if (strcmp(cmd, "mb_cfg") == 0 && _snd) {
    ManualBeatConfig& mb = _snd->getManualBeatConfig();
    mb.enabled      = (doc["en"]|0) != 0;
    mb.source       = doc["src"]  | (uint8_t)BSRC_MANUAL;
    mb.bpm          = doc["bpm"]  | 120.0f;
    mb.program      = doc["prog"] | (uint8_t)MBPROG_PULSE;
    mb.subdiv       = doc["sub"]  | (uint8_t)SUBDIV_NORMAL;
    mb.intensity    = doc["int"]  | 0.8f;
    mb.colorEnabled = (doc["col"]|1) != 0;
    mb.buildBeats   = doc["bb"]   | 8;
    // Faza 3: krivulje + envelope
    mb.dimCurve     = doc["dc"]   | (uint8_t)DIM_LINEAR;
    mb.attackMs     = doc["atk"]  | (uint16_t)0;
    mb.decayMs      = doc["dcy"]  | (uint16_t)0;
    // FX simetrija
    mb.symmetry     = doc["sym"]  | (uint8_t)SYM_FORWARD;
    // Faza 4: paleta
    mb.palette      = doc["pal"]  | (uint8_t)PAL_RAINBOW;
    if (doc["cpal"].is<JsonArray>()) {
      JsonArray arr = doc["cpal"].as<JsonArray>();
      for (int i = 0; i < 4 && i < (int)arr.size(); i++)
        mb.customHues[i] = arr[i] | (uint16_t)0;
    }
    // Faza 6: per-group overrides
    if (doc["grp"].is<JsonArray>()) {
      // Reset all to inherit first
      for (int i = 0; i < MAX_GROUPS; i++) {
        mb.groupOverrides[i] = {GROUP_BEAT_INHERIT, GROUP_BEAT_INHERIT, GROUP_BEAT_INHERIT};
      }
      JsonArray ga = doc["grp"].as<JsonArray>();
      for (int i = 0; i < MAX_GROUPS && i < (int)ga.size(); i++) {
        JsonObject go = ga[i];
        mb.groupOverrides[i].program   = go["p"] | GROUP_BEAT_INHERIT;
        mb.groupOverrides[i].subdiv    = go["s"] | GROUP_BEAT_INHERIT;
        mb.groupOverrides[i].intensity = go["i"] | GROUP_BEAT_INHERIT;
      }
    }
    if (mb.bpm < 30) mb.bpm = 30;
    if (mb.bpm > 300) mb.bpm = 300;
    // Ableton Link: enable/disable based on source selection
    _snd->getLinkBeat().enable(mb.source == BSRC_LINK);
  }
  // Faza 5: program chain
  else if (strcmp(cmd, "mb_chain") == 0 && _snd) {
    ProgramChain& ch = _snd->getChain();
    ch.active = (doc["en"]|0) != 0;
    ch.count = 0;
    if (doc["entries"].is<JsonArray>()) {
      JsonArray arr = doc["entries"].as<JsonArray>();
      for (JsonObject e : arr) {
        if (ch.count >= MAX_CHAIN_ENTRIES) break;
        ch.entries[ch.count].program = e["p"] | 0;
        ch.entries[ch.count].durationBeats = e["d"] | 8;
        ch.count++;
      }
    }
  }
  // Pixel Mapper commands
  #if defined(CONFIG_IDF_TARGET_ESP32S3)
  else if (strcmp(cmd, "px_cfg") == 0 && _pxMap) {
    PixelMapConfig pc = _pxMap->getConfig();
    if (doc.containsKey("en"))   pc.enabled    = (doc["en"]|0) != 0;
    if (doc.containsKey("cnt"))  pc.ledCount   = doc["cnt"] | 30;
    if (doc.containsKey("brt"))  pc.brightness = doc["brt"] | 128;
    if (doc.containsKey("mode")) pc.mode       = (PixelMapMode)(doc["mode"]|0);
    if (doc.containsKey("fx"))   pc.fixtureIdx = doc["fx"] | 0;
    if (doc.containsKey("gmask")) pc.groupMask = doc["gmask"] | 0;
    _pxMap->setConfig(pc);
  }
  else if (strcmp(cmd, "px_save") == 0 && _pxMap) { _pxMap->saveConfig(); }
  #endif
  // ESP-NOW commands
  else if (strcmp(cmd, "now_en") == 0 && _espNow) {
    _espNow->setEnabled((doc["v"]|0) != 0);
  }
  else if (strcmp(cmd, "now_add") == 0 && _espNow) {
    // MAC as string "AA:BB:CC:DD:EE:FF"
    const char* macStr = doc["mac"];
    if (macStr) {
      uint8_t mac[6];
      sscanf(macStr, "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx",
             &mac[0],&mac[1],&mac[2],&mac[3],&mac[4],&mac[5]);
      _espNow->addPeer(mac, doc["name"] | "Slave");
    }
  }
  else if (strcmp(cmd, "now_rm") == 0 && _espNow) {
    _espNow->removePeer(doc["i"]|0);
  }
  else if (strcmp(cmd, "now_save") == 0 && _espNow) { _espNow->saveConfig(); }

  _mix->unlock();
}

// ============================================================================
//  REST API handlers
// ============================================================================

// Shared POST body buffer — avoids heap fragmentation from static String
#define POST_BUF_SIZE 1536
static char _postBuf[POST_BUF_SIZE];
static size_t _postLen = 0;
#define POST_ACCUM(data,len,index,total) \
  if(index==0)_postLen=0; \
  size_t cpLen=min((size_t)(len),(size_t)(POST_BUF_SIZE-1-_postLen)); \
  memcpy(_postBuf+_postLen,data,cpLen); _postLen+=cpLen; _postBuf[_postLen]=0; \
  if(index+len<total)return;

static void apiGetConfig(AsyncWebServerRequest* req) {
  JsonDocument doc;
  doc["hostname"]=_cfg->hostname; doc["universe"]=_cfg->universe; doc["channelCount"]=_cfg->channelCount;
  JsonArray aps=doc["wifiAPs"].to<JsonArray>();
  for(int i=0;i<MAX_WIFI_APS;i++){
    if(_cfg->wifiAPs[i].ssid[0]=='\0') continue;
    JsonObject ap=aps.add<JsonObject>(); ap["ssid"]=_cfg->wifiAPs[i].ssid; ap["password"]=_cfg->wifiAPs[i].password;
  }
  doc["dhcp"]=_cfg->dhcp; doc["staticIp"]=_cfg->staticIp;
  doc["staticGw"]=_cfg->staticGw; doc["staticSn"]=_cfg->staticSn; doc["audioSource"]=_cfg->audioSource;
  doc["authEnabled"]=_cfg->authEnabled; doc["authUser"]=_cfg->authUser;
  doc["artnetTimeoutSec"]=_cfg->artnetTimeoutSec; doc["artnetPrimaryMode"]=_cfg->artnetPrimaryMode;
  doc["artnetOutEnabled"]=_cfg->artnetOutEnabled; doc["sacnEnabled"]=_cfg->sacnEnabled;
  doc["version"]=FW_VERSION " " __DATE__; doc["ip"]=WiFi.localIP().toString(); doc["mac"]=WiFi.macAddress();
  doc["mdns"]=String("http://") + _cfg->hostname + ".local";
  String json; serializeJson(doc,json); req->send(200,"application/json",json);
}

static void apiPostConfig(AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t index, size_t total) {
  POST_ACCUM(data,len,index,total)
  JsonDocument doc; if(deserializeJson(doc,_postBuf)){req->send(400,"application/json","{\"ok\":false}");return;}
  strlcpy(_cfg->hostname,doc["hostname"]|_cfg->hostname,sizeof(_cfg->hostname));
  _cfg->universe=doc["universe"]|_cfg->universe; _cfg->channelCount=doc["channelCount"]|_cfg->channelCount;

  // WiFi APs
  memset(_cfg->wifiAPs, 0, sizeof(_cfg->wifiAPs));
  if(doc["wifiAPs"].is<JsonArray>()){
    JsonArray arr=doc["wifiAPs"].as<JsonArray>(); int i=0;
    for(JsonObject ap:arr){
      if(i>=MAX_WIFI_APS) break;
      strlcpy(_cfg->wifiAPs[i].ssid,ap["ssid"]|"",sizeof(_cfg->wifiAPs[i].ssid));
      strlcpy(_cfg->wifiAPs[i].password,ap["password"]|"",sizeof(_cfg->wifiAPs[i].password));
      i++;
    }
  }

  _cfg->dhcp=doc["dhcp"]|_cfg->dhcp;
  strlcpy(_cfg->staticIp,doc["staticIp"]|_cfg->staticIp,sizeof(_cfg->staticIp));
  strlcpy(_cfg->staticGw,doc["staticGw"]|_cfg->staticGw,sizeof(_cfg->staticGw));
  strlcpy(_cfg->staticSn,doc["staticSn"]|_cfg->staticSn,sizeof(_cfg->staticSn));
  _cfg->audioSource=doc["audioSource"]|_cfg->audioSource;
  _cfg->authEnabled=doc["authEnabled"]|_cfg->authEnabled;
  if(doc["authUser"].is<const char*>()) strlcpy(_cfg->authUser,doc["authUser"],sizeof(_cfg->authUser));
  if(doc["authPass"].is<const char*>()&&strlen(doc["authPass"])>0) strlcpy(_cfg->authPass,doc["authPass"],sizeof(_cfg->authPass));
  if(!doc["artnetTimeoutSec"].isNull()) _cfg->artnetTimeoutSec=doc["artnetTimeoutSec"]|_cfg->artnetTimeoutSec;
  if(!doc["artnetPrimaryMode"].isNull()) _cfg->artnetPrimaryMode=doc["artnetPrimaryMode"]|false;
  if(!doc["artnetOutEnabled"].isNull()) _cfg->artnetOutEnabled=doc["artnetOutEnabled"]|false;
  if(!doc["sacnEnabled"].isNull()) _cfg->sacnEnabled=doc["sacnEnabled"]|false;

  bool ok=configSave(*_cfg); req->send(200,"application/json",ok?"{\"ok\":true}":"{\"ok\":false}");
  if(ok){delay(500);ESP.restart();}
}

static void apiGetFixtures(AsyncWebServerRequest* req) {
  JsonDocument doc;
  JsonArray fArr=doc["fixtures"].to<JsonArray>();
  for(int i=0;i<MAX_FIXTURES;i++){
    const PatchEntry* fx=_fix->getFixture(i); if(!fx||!fx->active)continue;
    JsonObject o=fArr.add<JsonObject>(); o["idx"]=i; o["name"]=fx->name; o["profileId"]=fx->profileId;
    o["dmxAddress"]=fx->dmxAddress; o["groupMask"]=fx->groupMask; o["soundReactive"]=fx->soundReactive;
    if(fx->invertPan) o["invertPan"]=true; if(fx->invertTilt) o["invertTilt"]=true;
    if(fx->panMin>0) o["panMin"]=fx->panMin; if(fx->panMax<255) o["panMax"]=fx->panMax;
    if(fx->tiltMin>0) o["tiltMin"]=fx->tiltMin; if(fx->tiltMax<255) o["tiltMax"]=fx->tiltMax;
    if(fx->profileIndex>=0){
      const FixtureProfile* p=_fix->getProfile(fx->profileIndex);
      if(p){
        if(p->zoomMin||p->zoomMax){o["zoomMin"]=p->zoomMin;o["zoomMax"]=p->zoomMax;}
        JsonArray cArr=o["channels"].to<JsonArray>();
        const uint8_t* vals=(_mix->getMode()==CTRL_ARTNET)?_mix->getDmxOutput():_mix->getManualValues();
        for(int c=0;c<p->channelCount;c++){JsonObject ch=cArr.add<JsonObject>(); ch["name"]=p->channels[c].name;
          ch["type"]=p->channels[c].type; ch["default"]=p->channels[c].defaultValue;
          uint16_t addr=fx->dmxAddress+c-1;
          ch["currentValue"]=(addr<DMX_MAX_CHANNELS)?vals[addr]:0;
          if(p->channels[c].rangeCount>0){JsonArray rArr=ch["ranges"].to<JsonArray>();
            for(int r=0;r<p->channels[c].rangeCount;r++){JsonObject ro=rArr.add<JsonObject>();
              ro["from"]=p->channels[c].ranges[r].from; ro["to"]=p->channels[c].ranges[r].to; ro["label"]=p->channels[c].ranges[r].label;}}}}}
  }
  JsonArray pArr=doc["profiles"].to<JsonArray>();
  for(int i=0;i<_fix->getProfileCount();i++){const FixtureProfile* p=_fix->getProfile(i);if(!p)continue;
    JsonObject o=pArr.add<JsonObject>(); o["id"]=p->id; o["name"]=p->name; o["channelCount"]=p->channelCount;}
  JsonArray gArr=doc["groups"].to<JsonArray>();
  for(int i=0;i<MAX_GROUPS;i++){const GroupDef* g=_fix->getGroup(i); gArr.add(g&&g->active?g->name:"");}
  String json; serializeJson(doc,json); req->send(200,"application/json",json);
}

static void apiPostFixtures(AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t index, size_t total) {
  POST_ACCUM(data,len,index,total)
  JsonDocument doc; if(deserializeJson(doc,_postBuf)){req->send(400,"application/json","{\"ok\":false}");return;}
  const char* action=doc["action"]; bool ok=false;
  if(strcmp(action,"add")==0){JsonObject fx=doc["fixture"];
    ok=_fix->addFixture(fx["name"]|"?",fx["profileId"]|"",fx["dmxAddress"]|1,fx["groupMask"]|0,fx["soundReactive"]|false);
    // Apliciraj default vrednosti kanalov (npr. Pan=128, Tilt=128 za center)
    if(ok){
      for(int i=0;i<MAX_FIXTURES;i++){
        const PatchEntry* pe=_fix->getFixture(i);
        if(!pe||!pe->active||pe->profileIndex<0)continue;
        if(strcmp(pe->profileId,fx["profileId"]|"")!=0||pe->dmxAddress!=(fx["dmxAddress"]|1))continue;
        const FixtureProfile* p=_fix->getProfile(pe->profileIndex);
        if(p){for(int c=0;c<p->channelCount;c++){if(p->channels[c].defaultValue)_mix->setFixtureChannel(i,c,p->channels[c].defaultValue);}}
        break;
      }
    }}
  else if(strcmp(action,"remove")==0) ok=_fix->removeFixture(doc["index"]|-1);
  else if(strcmp(action,"update")==0){
    int idx=doc["index"]|-1;
    PatchEntry* fx=_fix->getFixtureMut(idx);
    if(fx){
      if(!doc["fixture"]["name"].isNull()) strlcpy(fx->name, doc["fixture"]["name"]|"", sizeof(fx->name));
      if(!doc["fixture"]["dmxAddress"].isNull()) fx->dmxAddress=doc["fixture"]["dmxAddress"]|1;
      if(!doc["fixture"]["groupMask"].isNull()) fx->groupMask=doc["fixture"]["groupMask"]|0;
      if(!doc["fixture"]["soundReactive"].isNull()) fx->soundReactive=doc["fixture"]["soundReactive"]|false;
      if(!doc["fixture"]["invertPan"].isNull()) fx->invertPan=doc["fixture"]["invertPan"]|false;
      if(!doc["fixture"]["invertTilt"].isNull()) fx->invertTilt=doc["fixture"]["invertTilt"]|false;
      if(!doc["fixture"]["panMin"].isNull()) fx->panMin=doc["fixture"]["panMin"]|0;
      if(!doc["fixture"]["panMax"].isNull()) fx->panMax=doc["fixture"]["panMax"]|255;
      if(!doc["fixture"]["tiltMin"].isNull()) fx->tiltMin=doc["fixture"]["tiltMin"]|0;
      if(!doc["fixture"]["tiltMax"].isNull()) fx->tiltMax=doc["fixture"]["tiltMax"]|255;
      ok=true;
    }
  }
  if(ok) _fix->savePatch(); req->send(200,"application/json",ok?"{\"ok\":true}":"{\"ok\":false}");
}

static void apiProfileUpload(AsyncWebServerRequest* req, const String& filename, size_t index, uint8_t* data, size_t len, bool final) {
  static File uploadFile;
  if(index==0){String path=String(PATH_PROFILES_DIR)+"/"+filename; uploadFile=LittleFS.open(path,"w");}
  if(uploadFile)uploadFile.write(data,len);
  if(final){if(uploadFile)uploadFile.close(); _fix->loadAllProfiles(); _fix->resolvePatchProfiles(); req->send(200,"application/json","{\"ok\":true}");}
}

static void apiDeleteProfile(AsyncWebServerRequest* req) {
  if(!req->hasParam("id")){req->send(400);return;}
  bool ok=_fix->deleteProfile(req->getParam("id")->value().c_str());
  req->send(200,"application/json",ok?"{\"ok\":true}":"{\"ok\":false}");
}

static void apiPostGroups(AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t index, size_t total) {
  POST_ACCUM(data,len,index,total)
  JsonDocument doc; if(deserializeJson(doc,_postBuf)){req->send(400);return;}
  int idx=doc["index"]|-1; const char* name=doc["name"]|"";
  if(idx>=0&&idx<MAX_GROUPS){if(strlen(name)>0)_fix->setGroup(idx,name);else _fix->clearGroup(idx);_fix->saveGroups();}
  req->send(200,"application/json","{\"ok\":true}");
}

// ============================================================================
//  SCENE API
// ============================================================================

static void apiGetScenes(AsyncWebServerRequest* req) {
  JsonDocument doc; JsonArray arr=doc["scenes"].to<JsonArray>();
  for(int i=0;i<MAX_SCENES;i++){const Scene* sc=_scn->getScene(i);
    if(sc){JsonObject o=arr.add<JsonObject>();o["slot"]=i;o["name"]=sc->name;
      // Preview: izračunaj barvo za vsak fixture
      JsonArray prev=o["prev"].to<JsonArray>();
      for(int fi=0;fi<MAX_FIXTURES;fi++){
        const PatchEntry* fx=_fix->getFixture(fi);
        if(!fx||!fx->active||fx->profileIndex<0){prev.add(nullptr);continue;}
        uint8_t r=0,g=0,b=0,dim=255;
        uint8_t chCnt=_fix->fixtureChannelCount(fi);
        for(int c=0;c<chCnt;c++){
          const ChannelDef* ch=_fix->fixtureChannel(fi,c);
          if(!ch)continue;
          uint16_t addr=fx->dmxAddress+c-1;
          uint8_t v=addr<DMX_MAX_CHANNELS?sc->dmx[addr]:0;
          if(ch->type==CH_INTENSITY)dim=v;
          else if(ch->type==CH_COLOR_R)r=v;
          else if(ch->type==CH_COLOR_G)g=v;
          else if(ch->type==CH_COLOR_B)b=v;
        }
        // Apliciraj dimmer
        r=((uint16_t)r*dim)/255; g=((uint16_t)g*dim)/255; b=((uint16_t)b*dim)/255;
        JsonArray c=prev.add<JsonArray>(); c.add(r); c.add(g); c.add(b);
      }
    }
  }
  String json; serializeJson(doc,json); req->send(200,"application/json",json);
}

static void apiPostScenes(AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t index, size_t total) {
  POST_ACCUM(data,len,index,total)
  JsonDocument doc; if(deserializeJson(doc,_postBuf)){req->send(400,"application/json","{\"ok\":false}");return;}
  const char* action=doc["action"]; bool ok=false; int slot=-1;
  if(strcmp(action,"save")==0){slot=_scn->findFreeSlot();if(slot>=0)ok=_mix->saveCurrentAsScene(slot,doc["name"]|"Scena");}
  else if(strcmp(action,"overwrite")==0){slot=doc["slot"]|-1;if(slot>=0)ok=_mix->saveCurrentAsScene(slot,doc["name"]|"Scena");}
  else if(strcmp(action,"rename")==0){slot=doc["slot"]|-1;if(slot>=0)ok=_scn->renameScene(slot,doc["name"]|"");}
  else if(strcmp(action,"delete")==0){slot=doc["slot"]|-1;if(slot>=0)ok=_scn->deleteScene(slot);}
  JsonDocument resp; resp["ok"]=ok; resp["slot"]=slot; String json; serializeJson(resp,json); req->send(200,"application/json",json);
}

// ============================================================================
//  SOUND API
// ============================================================================

static void apiGetSoundRules(AsyncWebServerRequest* req) {
  if(!_snd){req->send(500);return;}
  JsonDocument doc; JsonArray arr=doc["rules"].to<JsonArray>();
  for(int i=0;i<STL_MAX_RULES;i++){
    const STLRule* r=_snd->getRule(i);
    JsonObject o=arr.add<JsonObject>();
    if(r){o["active"]=true; o["fixtureIdx"]=r->fixtureIdx; o["channelIdx"]=r->channelIdx;
      o["freqLow"]=r->freqLow; o["freqHigh"]=r->freqHigh; o["outMin"]=r->outMin;
      o["outMax"]=r->outMax; o["curve"]=r->curve; o["attackMs"]=r->attackMs; o["decayMs"]=r->decayMs;}
    else o["active"]=false;
  }
  String json; serializeJson(doc,json); req->send(200,"application/json",json);
}

static void apiPostSoundRules(AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t index, size_t total) {
  POST_ACCUM(data,len,index,total)
  if(!_snd){req->send(500,"application/json","{\"ok\":false}");return;}
  JsonDocument doc; if(deserializeJson(doc,_postBuf)){req->send(400,"application/json","{\"ok\":false}");return;}
  const char* action=doc["action"]; bool ok=false;
  if(strcmp(action,"add")==0){
    // Poišči prvi prosti slot
    for(int i=0;i<STL_MAX_RULES;i++){
      if(!_snd->getRule(i)){
        JsonObject r=doc["rule"];
        STLRule rule={}; rule.active=true; rule.fixtureIdx=r["fixtureIdx"]|0; rule.channelIdx=r["channelIdx"]|0;
        rule.freqLow=r["freqLow"]|60; rule.freqHigh=r["freqHigh"]|250; rule.outMin=r["outMin"]|0;
        rule.outMax=r["outMax"]|255; rule.curve=r["curve"]|0; rule.attackMs=r["attackMs"]|5; rule.decayMs=r["decayMs"]|20;
        ok=_snd->setRule(i,rule); break;
      }
    }
  }
  else if(strcmp(action,"delete")==0) ok=_snd->clearRule(doc["index"]|-1);
  if(ok) _snd->saveConfig();  // Persistiraj po spremembi
  req->send(200,"application/json",ok?"{\"ok\":true}":"{\"ok\":false}");
}

static void apiFactoryReset(AsyncWebServerRequest* req) {
  LittleFS.format(); req->send(200,"application/json","{\"ok\":true}"); delay(500); ESP.restart();
}

// ============================================================================
//  KONFIGURACIJE — shranjevanje/nalaganje celotne postavitve
// ============================================================================

static String sanitizeFilename(const char* name) {
  String out;
  for (int i = 0; name[i] && i < 32; i++) {
    char c = name[i];
    if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-' || c == '_')
      out += c;
    else if (c == ' ')
      out += '_';
  }
  if (out.length() == 0) out = "config";
  return out;
}

// Helper: encode 512 bytes DMX → base64 string
static String dmxToBase64(const uint8_t* dmx) {
  size_t olen = 0;
  // First call to get output length
  mbedtls_base64_encode(NULL, 0, &olen, dmx, DMX_MAX_CHANNELS);
  char* buf = (char*)malloc(olen + 1);
  if (!buf) return "";
  mbedtls_base64_encode((unsigned char*)buf, olen + 1, &olen, dmx, DMX_MAX_CHANNELS);
  buf[olen] = '\0';
  String s(buf);
  free(buf);
  return s;
}

// Helper: decode base64 → 512 bytes DMX
static bool base64ToDmx(const char* b64, uint8_t* dmx) {
  size_t olen = 0;
  int ret = mbedtls_base64_decode(dmx, DMX_MAX_CHANNELS, &olen, (const unsigned char*)b64, strlen(b64));
  return (ret == 0 && olen == DMX_MAX_CHANNELS);
}

// GET /api/cfglist — seznam konfiguracij + storage info
static void apiGetCfgList(AsyncWebServerRequest* req) {
  if (!LittleFS.exists(PATH_CONFIGS_DIR)) LittleFS.mkdir(PATH_CONFIGS_DIR);

  JsonDocument doc;
  JsonArray arr = doc["configs"].to<JsonArray>();

  File dir = LittleFS.open(PATH_CONFIGS_DIR);
  if (dir && dir.isDirectory()) {
    File f = dir.openNextFile();
    while (f) {
      String fname = f.name();
      int lastSlash = fname.lastIndexOf('/');
      if (lastSlash >= 0) fname = fname.substring(lastSlash + 1);

      if (!f.isDirectory() && fname.endsWith(".json")) {
        JsonObject o = arr.add<JsonObject>();
        o["file"] = fname;
        o["size"] = String(f.size() / 1024.0f, 1) + "KB";

        // Read only name and date (use filter to avoid parsing huge scene data)
        JsonDocument filter;
        filter["name"] = true;
        filter["created"] = true;
        JsonDocument meta;
        DeserializationError err = deserializeJson(meta, f, DeserializationOption::Filter(filter));
        if (!err) {
          o["name"] = meta["name"] | fname.c_str();
          o["date"] = meta["created"] | "?";
        } else {
          o["name"] = fname;
          o["date"] = "?";
        }
      }
      f = dir.openNextFile();
    }
  }

  doc["totalBytes"] = LittleFS.totalBytes();
  doc["usedBytes"] = LittleFS.usedBytes();
  doc["freeBytes"] = LittleFS.totalBytes() - LittleFS.usedBytes();

  String json;
  serializeJson(doc, json);
  req->send(200, "application/json", json);
}

// POST /api/cfgsave — shrani trenutno stanje kot konfig
static void apiCfgSave(AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t index, size_t total) {
  POST_ACCUM(data, len, index, total)
  JsonDocument inp;
  if (deserializeJson(inp, _postBuf)) { req->send(400, "application/json", "{\"ok\":false,\"error\":\"Invalid JSON\"}"); return; }

  const char* name = inp["name"] | "config";
  String safeName = sanitizeFilename(name);

  if (!LittleFS.exists(PATH_CONFIGS_DIR)) LittleFS.mkdir(PATH_CONFIGS_DIR);

  // Check free space (rough estimate: ~30KB per config)
  size_t freeBytes = LittleFS.totalBytes() - LittleFS.usedBytes();
  if (freeBytes < 35000) {
    req->send(200, "application/json", "{\"ok\":false,\"error\":\"Premalo prostora na ESP\"}");
    return;
  }

  // Build config JSON
  JsonDocument cfg;
  cfg["version"] = 1;
  cfg["name"] = name;
  cfg["created"] = inp["date"] | "?";
  cfg["firmware"] = "2.0";

  // --- Patch ---
  JsonArray pArr = cfg["patch"].to<JsonArray>();
  for (int i = 0; i < MAX_FIXTURES; i++) {
    const PatchEntry* fx = _fix->getFixture(i);
    if (!fx || !fx->active) continue;
    JsonObject o = pArr.add<JsonObject>();
    o["name"] = fx->name;
    o["profileId"] = fx->profileId;
    o["dmxAddress"] = fx->dmxAddress;
    o["groupMask"] = fx->groupMask;
    o["soundReactive"] = fx->soundReactive;
    if (fx->invertPan)    o["invertPan"]  = true;
    if (fx->invertTilt)   o["invertTilt"] = true;
    if (fx->panMin > 0)   o["panMin"]     = fx->panMin;
    if (fx->panMax < 255) o["panMax"]     = fx->panMax;
    if (fx->tiltMin > 0)  o["tiltMin"]    = fx->tiltMin;
    if (fx->tiltMax < 255)o["tiltMax"]     = fx->tiltMax;
  }

  // --- Groups ---
  JsonArray gArr = cfg["groups"].to<JsonArray>();
  for (int i = 0; i < MAX_GROUPS; i++) {
    const GroupDef* g = _fix->getGroup(i);
    gArr.add(g && g->active ? g->name : "");
  }

  // --- Scenes (base64 DMX) ---
  JsonArray sArr = cfg["scenes"].to<JsonArray>();
  for (int i = 0; i < MAX_SCENES; i++) {
    const Scene* s = _scn->getScene(i);
    if (!s || !s->valid) continue;
    JsonObject o = sArr.add<JsonObject>();
    o["slot"] = i;
    o["name"] = s->name;
    o["dmx"] = dmxToBase64(s->dmx);
  }

  // --- Sound config ---
  if (_snd) {
    JsonObject sndObj = cfg["sound"].to<JsonObject>();
    const STLEasyConfig& easy = _snd->getEasyConfig();
    sndObj["enabled"] = easy.enabled;
    sndObj["sensitivity"] = easy.sensitivity;
    sndObj["soundAmount"] = easy.soundAmount;
    sndObj["bassIntensity"] = easy.bassIntensity;
    sndObj["midColor"] = easy.midColor;
    sndObj["highStrobe"] = easy.highStrobe;
    sndObj["beatBump"] = easy.beatBump;
    sndObj["preset"] = easy.preset;
    sndObj["beatSync"] = easy.beatSync;
    sndObj["beatPhase"] = easy.beatPhase;

    JsonArray zones = sndObj["zones"].to<JsonArray>();
    for (int i = 0; i < MAX_FIXTURES; i++) zones.add(easy.zones[i]);

    JsonArray rules = sndObj["rules"].to<JsonArray>();
    for (int i = 0; i < STL_MAX_RULES; i++) {
      const STLRule* r = _snd->getRule(i);
      if (!r || !r->active) continue;
      JsonObject ro = rules.add<JsonObject>();
      ro["fixture"] = r->fixtureIdx; ro["channel"] = r->channelIdx;
      ro["freqLow"] = r->freqLow; ro["freqHigh"] = r->freqHigh;
      ro["outMin"] = r->outMin; ro["outMax"] = r->outMax;
      ro["curve"] = r->curve; ro["attackMs"] = r->attackMs; ro["decayMs"] = r->decayMs;
    }

    // AGC config
    const STLAgcConfig& agc = _snd->getAgcConfig();
    JsonObject agcObj = sndObj["agc"].to<JsonObject>();
    agcObj["agcSpeed"] = agc.agcSpeed;
    agcObj["noiseGate"] = agc.noiseGate;
    JsonArray bgArr = agcObj["bandGains"].to<JsonArray>();
    for (int i = 0; i < STL_BAND_COUNT; i++) bgArr.add(agc.bandGains[i]);
  }

  // --- Mixer ---
  JsonObject mixObj = cfg["mixer"].to<JsonObject>();
  mixObj["masterDimmer"] = _mix->getMasterDimmer();

  // --- Profili (polni JSON za backup/restore) ---
  JsonArray profArr = cfg["profiles"].to<JsonArray>();
  File profDir = LittleFS.open(PATH_PROFILES_DIR);
  if (profDir && profDir.isDirectory()) {
    File pf = profDir.openNextFile();
    while (pf) {
      String pfName = pf.name();
      int lastSlash = pfName.lastIndexOf('/');
      if (lastSlash >= 0) pfName = pfName.substring(lastSlash + 1);
      if (!pf.isDirectory() && pfName.endsWith(".json") && pf.size() < 4096) {
        JsonObject po = profArr.add<JsonObject>();
        po["filename"] = pfName;
        // Preberi vsebino profila kot string
        String content = pf.readString();
        po["data"] = content;
      }
      pf = profDir.openNextFile();
    }
  }

  // Write to file
  String path = String(PATH_CONFIGS_DIR) + "/" + safeName + ".json";
  File f = LittleFS.open(path, "w");
  if (!f) { req->send(200, "application/json", "{\"ok\":false,\"error\":\"Napaka pri pisanju\"}"); return; }
  serializeJson(cfg, f);
  f.close();

  Serial.printf("[CFG] Konfiguracija '%s' shranjena (%d bajtov)\n", name, measureJson(cfg));
  req->send(200, "application/json", "{\"ok\":true}");
}

// POST /api/cfgload — naloži konfig iz LittleFS
static void apiCfgLoad(AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t index, size_t total) {
  POST_ACCUM(data, len, index, total)
  JsonDocument inp;
  if (deserializeJson(inp, _postBuf)) { req->send(400); return; }

  const char* file = inp["file"] | "";
  String path = String(PATH_CONFIGS_DIR) + "/" + file;

  File f = LittleFS.open(path, "r");
  if (!f) { req->send(200, "application/json", "{\"ok\":false,\"error\":\"Datoteka ne obstaja\"}"); return; }

  JsonDocument cfg;
  if (deserializeJson(cfg, f)) { f.close(); req->send(200, "application/json", "{\"ok\":false,\"error\":\"Neveljaven JSON\"}"); return; }
  f.close();

  int warnings = 0;

  // --- Restore Profiles (pred patch-em, da so na voljo ob resolve) ---
  if (cfg.containsKey("profiles")) {
    JsonArray profArr = cfg["profiles"].as<JsonArray>();
    if (profArr) {
      int restored = 0;
      for (JsonObject po : profArr) {
        const char* filename = po["filename"] | "";
        const char* data = po["data"] | "";
        if (filename[0] && data[0]) {
          String path = String(PATH_PROFILES_DIR) + "/" + filename;
          File pf = LittleFS.open(path, "w");
          if (pf) { pf.print(data); pf.close(); restored++; }
        }
      }
      if (restored > 0) {
        _fix->loadAllProfiles();  // Reload vseh profilov
        Serial.printf("[CFG] Obnovljenih %d profilov\n", restored);
      }
    }
  }

  // --- Load Patch ---
  // Clear all fixtures first
  for (int i = 0; i < MAX_FIXTURES; i++) {
    PatchEntry* fx = _fix->getFixtureMut(i);
    if (fx) fx->active = false;
  }
  JsonArray pArr = cfg["patch"].as<JsonArray>();
  if (pArr) {
    int slot = 0;
    for (JsonObject o : pArr) {
      if (slot >= MAX_FIXTURES) break;
      _fix->addFixture(o["name"]|"?", o["profileId"]|"", o["dmxAddress"]|1, o["groupMask"]|0, o["soundReactive"]|false);
      // Pan/Tilt omejitve
      PatchEntry* pe = _fix->getFixtureMut(slot);
      if (pe) {
        pe->invertPan  = o["invertPan"]  | false;
        pe->invertTilt = o["invertTilt"] | false;
        pe->panMin     = o["panMin"]     | 0;
        pe->panMax     = o["panMax"]     | 255;
        pe->tiltMin    = o["tiltMin"]    | 0;
        pe->tiltMax    = o["tiltMax"]    | 255;
      }
      slot++;
    }
  }
  _fix->savePatch();
  _fix->resolvePatchProfiles();

  // Check for missing profiles
  for (int i = 0; i < MAX_FIXTURES; i++) {
    const PatchEntry* fx = _fix->getFixture(i);
    if (fx && fx->active && fx->profileIndex < 0) warnings++;
  }

  // --- Load Groups ---
  JsonArray gArr = cfg["groups"].as<JsonArray>();
  if (gArr) {
    int i = 0;
    for (JsonVariant v : gArr) {
      if (i >= MAX_GROUPS) break;
      const char* n = v.as<const char*>();
      if (n && n[0]) _fix->setGroup(i, n); else _fix->clearGroup(i);
      i++;
    }
    _fix->saveGroups();
  }

  // --- Load Scenes ---
  // Delete all existing scenes first
  for (int i = 0; i < MAX_SCENES; i++) _scn->deleteScene(i);
  JsonArray sArr = cfg["scenes"].as<JsonArray>();
  if (sArr) {
    for (JsonObject o : sArr) {
      int slot = o["slot"] | -1;
      if (slot < 0 || slot >= MAX_SCENES) continue;
      const char* name = o["name"] | "?";
      const char* b64 = o["dmx"] | "";
      uint8_t dmx[DMX_MAX_CHANNELS];
      memset(dmx, 0, sizeof(dmx));
      if (base64ToDmx(b64, dmx)) {
        _scn->saveScene(slot, name, dmx);
      }
    }
  }

  // --- Load Sound ---
  if (_snd && cfg.containsKey("sound")) {
    JsonObject sndObj = cfg["sound"];
    STLEasyConfig easy = _snd->getEasyConfig();
    easy.enabled = sndObj["enabled"] | false;
    easy.sensitivity = sndObj["sensitivity"] | 1.0f;
    easy.soundAmount = sndObj["soundAmount"] | 0.5f;
    easy.bassIntensity = sndObj["bassIntensity"] | true;
    easy.midColor = sndObj["midColor"] | true;
    easy.highStrobe = sndObj["highStrobe"] | true;
    easy.beatBump = sndObj["beatBump"] | true;
    easy.preset = sndObj["preset"] | 0;
    easy.beatSync = sndObj["beatSync"] | false;
    easy.beatPhase = sndObj["beatPhase"] | 0.0f;
    JsonArray zones = sndObj["zones"].as<JsonArray>();
    if (zones) { int i = 0; for (JsonVariant v : zones) { if (i >= MAX_FIXTURES) break; easy.zones[i] = v | 0; i++; } }
    _snd->setEasyConfig(easy);

    // Rules
    for (int i = 0; i < STL_MAX_RULES; i++) _snd->clearRule(i);
    JsonArray rules = sndObj["rules"].as<JsonArray>();
    if (rules) {
      int i = 0;
      for (JsonObject ro : rules) {
        if (i >= STL_MAX_RULES) break;
        STLRule r = {};
        r.active = true;
        r.fixtureIdx = ro["fixture"] | 0; r.channelIdx = ro["channel"] | 0;
        r.freqLow = ro["freqLow"] | 60; r.freqHigh = ro["freqHigh"] | 250;
        r.outMin = ro["outMin"] | 0; r.outMax = ro["outMax"] | 255;
        r.curve = ro["curve"] | 0; r.attackMs = ro["attackMs"] | 5; r.decayMs = ro["decayMs"] | 20;
        _snd->setRule(i, r);
        i++;
      }
    }

    // AGC config
    if (sndObj.containsKey("agc")) {
      JsonObject agcObj = sndObj["agc"];
      STLAgcConfig agc = STL_AGC_DEFAULTS;
      agc.agcSpeed  = agcObj["agcSpeed"]  | 0.5f;
      agc.noiseGate = agcObj["noiseGate"] | 0.3f;
      JsonArray bgArr = agcObj["bandGains"].as<JsonArray>();
      if (bgArr) {
        int j = 0;
        for (JsonVariant v : bgArr) {
          if (j >= STL_BAND_COUNT) break;
          agc.bandGains[j] = v | 1.0f;
          j++;
        }
      }
      _snd->setAgcConfig(agc);
    }

    _snd->saveConfig();
  }

  // --- Load Mixer ---
  if (cfg.containsKey("mixer")) {
    _mix->setMasterDimmer(cfg["mixer"]["masterDimmer"] | 255);
  }

  Serial.printf("[CFG] Konfiguracija naložena iz '%s', opozorila: %d\n", file, warnings);

  JsonDocument resp;
  resp["ok"] = true;
  resp["warnings"] = warnings;
  String json; serializeJson(resp, json);
  req->send(200, "application/json", json);
}

// POST /api/cfgdelete
static void apiCfgDelete(AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t index, size_t total) {
  POST_ACCUM(data, len, index, total)
  JsonDocument inp;
  if (deserializeJson(inp, _postBuf)) { req->send(400); return; }
  String path = String(PATH_CONFIGS_DIR) + "/" + (inp["file"] | "");
  bool ok = LittleFS.remove(path);
  req->send(200, "application/json", ok ? "{\"ok\":true}" : "{\"ok\":false}");
}

// GET /api/cfgdownload?f=name.json — prenesi datoteko
static void apiCfgDownload(AsyncWebServerRequest* req) {
  if (!req->hasParam("f")) { req->send(400); return; }
  String file = req->getParam("f")->value();
  String path = String(PATH_CONFIGS_DIR) + "/" + file;
  if (!LittleFS.exists(path)) { req->send(404); return; }
  req->send(LittleFS, path, "application/json", true);  // true = download
}

// POST /api/cfgupload — naloži konfig datoteko na ESP (ne aplicira)
static void apiCfgUpload(AsyncWebServerRequest* req, const String& filename, size_t index, uint8_t* data, size_t len, bool final) {
  static File uploadFile;
  if (index == 0) {
    if (!LittleFS.exists(PATH_CONFIGS_DIR)) LittleFS.mkdir(PATH_CONFIGS_DIR);
    String path = String(PATH_CONFIGS_DIR) + "/" + filename;
    uploadFile = LittleFS.open(path, "w");
  }
  if (uploadFile) uploadFile.write(data, len);
  if (final) {
    if (uploadFile) uploadFile.close();
    req->send(200, "application/json", "{\"ok\":true}");
  }
}

// ============================================================================
//  WiFi skeniranje
// ============================================================================

static void apiWifiScan(AsyncWebServerRequest* req) {
  int n = WiFi.scanComplete();
  if (n == WIFI_SCAN_FAILED) {
    WiFi.scanNetworks(true);  // Async scan
    req->send(200, "application/json", "{\"scanning\":true,\"networks\":[]}");
    return;
  }
  if (n == WIFI_SCAN_RUNNING) {
    req->send(200, "application/json", "{\"scanning\":true,\"networks\":[]}");
    return;
  }

  JsonDocument doc;
  JsonArray arr = doc["networks"].to<JsonArray>();
  doc["scanning"] = false;
  for (int i = 0; i < n && i < 20; i++) {
    JsonObject o = arr.add<JsonObject>();
    o["ssid"] = WiFi.SSID(i);
    o["rssi"] = WiFi.RSSI(i);
    o["open"] = (WiFi.encryptionType(i) == WIFI_AUTH_OPEN);
  }
  WiFi.scanDelete();
  String json; serializeJson(doc, json);
  req->send(200, "application/json", json);
}

// ============================================================================
//  Avtentikacija — Basic Auth middleware
// ============================================================================

static bool checkAuth(AsyncWebServerRequest* req) {
  if (!_cfg || !_cfg->authEnabled || _cfg->authPass[0] == '\0') return true;  // Auth ni aktiven
  if (!req->authenticate(_cfg->authUser, _cfg->authPass)) {
    req->requestAuthentication("ArtNet DMX Node");
    return false;
  }
  return true;
}

// ============================================================================
//  JAVNE FUNKCIJE
// ============================================================================

// ============================================================================
//  LAYOUT API — 2D oder urejevalnik
// ============================================================================

#define LAYOUT_BUF_SIZE 5120
static char _layoutBuf[LAYOUT_BUF_SIZE];
static size_t _layoutBufLen = 0;

static void apiGetLayouts(AsyncWebServerRequest* req) {
  if (!checkAuth(req)) return;
  if (!LittleFS.exists("/layouts")) LittleFS.mkdir("/layouts");
  JsonDocument doc; JsonArray arr = doc.to<JsonArray>();
  File dir = LittleFS.open("/layouts");
  if (dir && dir.isDirectory()) {
    File f = dir.openNextFile();
    while (f) {
      if (!f.isDirectory()) {
        String n = f.name();
        int sl = n.lastIndexOf('/');
        if (sl >= 0) n = n.substring(sl + 1);
        if (n.endsWith(".json")) arr.add(n.substring(0, n.length()-5));
      }
      f = dir.openNextFile();
    }
  }
  String json; serializeJson(doc, json); req->send(200, "application/json", json);
}

static void apiGetLayout(AsyncWebServerRequest* req) {
  if (!checkAuth(req)) return;
  if (!req->hasParam("name")) { req->send(400,"application/json","{\"ok\":false}"); return; }
  String path = String("/layouts/") + req->getParam("name")->value() + ".json";
  if (!LittleFS.exists(path)) { req->send(404,"application/json","{\"ok\":false,\"error\":\"not found\"}"); return; }
  req->send(LittleFS, path, "application/json");
}

static void apiLayoutSave(AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t index, size_t total) {
  if (index == 0) _layoutBufLen = 0;
  size_t cpLen = min((size_t)len, (size_t)(LAYOUT_BUF_SIZE-1-_layoutBufLen));
  memcpy(_layoutBuf+_layoutBufLen, data, cpLen); _layoutBufLen+=cpLen; _layoutBuf[_layoutBufLen]=0;
  if (index+len < total) return;
  JsonDocument doc;
  if (deserializeJson(doc, _layoutBuf)) { req->send(400,"application/json","{\"ok\":false}"); return; }
  const char* name = doc["name"]; if (!name||!name[0]) { req->send(400,"application/json","{\"ok\":false}"); return; }
  if (!LittleFS.exists("/layouts")) LittleFS.mkdir("/layouts");
  String path = String("/layouts/") + name + ".json";
  File f = LittleFS.open(path, "w");
  if (!f) { req->send(500,"application/json","{\"ok\":false,\"error\":\"write failed\"}"); return; }
  f.write((const uint8_t*)_layoutBuf, _layoutBufLen); f.close();
  req->send(200, "application/json", "{\"ok\":true}");
}

static void apiLayoutDelete(AsyncWebServerRequest* req) {
  if (!checkAuth(req)) return;
  if (!req->hasParam("name")) {
    Serial.println("[LAY] Delete: missing 'name' param");
    req->send(400,"application/json","{\"ok\":false,\"error\":\"no name\"}"); return;
  }
  String name = req->getParam("name")->value();
  if (name.length() == 0) {
    Serial.println("[LAY] Delete: empty name");
    req->send(400,"application/json","{\"ok\":false,\"error\":\"empty name\"}"); return;
  }
  String path = String("/layouts/") + name + ".json";
  Serial.printf("[LAY] Delete: name='%s' path='%s'\n", name.c_str(), path.c_str());
  bool exists = LittleFS.exists(path);
  bool removed = exists && LittleFS.remove(path);
  Serial.printf("[LAY] exists=%d removed=%d\n", exists, removed);
  if (!exists) {
    // Izpiši dejanske datoteke za debug
    File dir = LittleFS.open("/layouts");
    if (dir && dir.isDirectory()) {
      Serial.println("[LAY] Files in /layouts/:");
      File f = dir.openNextFile();
      while (f) { Serial.printf("  '%s'\n", f.name()); f = dir.openNextFile(); }
    }
  }
  req->send(200, "application/json", removed?"{\"ok\":true}":"{\"ok\":false,\"error\":\"not found\"}");
}

// ============================================================================

void webSetLfoEngine(LfoEngine* lfo) { _lfo = lfo; }
void webSetShapeGenerator(ShapeGenerator* shapes) { _shapeGen = shapes; }
#if defined(CONFIG_IDF_TARGET_ESP32S3)
void webSetPixelMapper(PixelMapper* px) { _pxMap = px; }
#endif
void webSetEspNow(EspNowDmx* espNow) { _espNow = espNow; }

void webBegin(AsyncWebServer* server, AsyncWebSocket* ws,
              NodeConfig* cfg, FixtureEngine* fixtures, MixerEngine* mixer,
              SceneEngine* scenes, SoundEngine* sound, AudioInput* audio) {
  _cfg=cfg; _fix=fixtures; _mix=mixer; _scn=scenes; _snd=sound; _aud=audio; _ws=ws;
  ws->onEvent(onWsEvent); server->addHandler(ws);

  // Glavna stran — gzip kompresiran HTML (iz web_ui_gz.h)
  server->on("/",HTTP_GET,[](AsyncWebServerRequest* req){
    Serial.printf("[WEB] GET / od %s (heap: %d)\n", req->client()->remoteIP().toString().c_str(), esp_get_free_heap_size());
    AsyncWebServerResponse* resp = req->beginResponse_P(200, "text/html", HTML_PAGE_GZ, HTML_PAGE_GZ_LEN);
    resp->addHeader("Content-Encoding", "gzip");
    resp->addHeader("Cache-Control", "no-cache, no-store, must-revalidate");
    req->send(resp);
  });
  server->on("/api/config",HTTP_GET,[](AsyncWebServerRequest* req){if(!checkAuth(req))return;apiGetConfig(req);});
  server->on("/api/config",HTTP_POST,[](AsyncWebServerRequest* req){},NULL,apiPostConfig);
  server->on("/api/fixtures",HTTP_GET,[](AsyncWebServerRequest* req){if(!checkAuth(req))return;apiGetFixtures(req);});
  server->on("/api/fixtures",HTTP_POST,[](AsyncWebServerRequest* req){},NULL,apiPostFixtures);
  server->on("/api/profile/upload",HTTP_POST,[](AsyncWebServerRequest* req){},apiProfileUpload);
  server->on("/api/profile",HTTP_DELETE,apiDeleteProfile);
  server->on("/api/groups",HTTP_POST,[](AsyncWebServerRequest* req){},NULL,apiPostGroups);
  server->on("/api/scenes",HTTP_GET,[](AsyncWebServerRequest* req){if(!checkAuth(req))return;apiGetScenes(req);});
  server->on("/api/scenes",HTTP_POST,[](AsyncWebServerRequest* req){},NULL,apiPostScenes);
  server->on("/api/sound/rules",HTTP_GET,apiGetSoundRules);
  server->on("/api/sound/rules",HTTP_POST,[](AsyncWebServerRequest* req){},NULL,apiPostSoundRules);
  server->on("/api/factory-reset",HTTP_POST,apiFactoryReset);
  server->on("/api/wifiscan",HTTP_GET,[](AsyncWebServerRequest* req){if(!checkAuth(req))return;apiWifiScan(req);});

  // Cue list API
  server->on("/api/cuelist",HTTP_GET,[](AsyncWebServerRequest* req){
    if(!_scn){req->send(500);return;}
    JsonDocument doc;JsonArray arr=doc["cues"].to<JsonArray>();
    for(int i=0;i<_scn->getCueCount();i++){
      const CueEntry* c=_scn->getCue(i);if(!c)continue;
      JsonObject o=arr.add<JsonObject>();
      o["s"]=c->sceneSlot;o["f"]=c->fadeMs;o["a"]=c->autoFollowMs;o["l"]=c->label;
    }
    String json;serializeJson(doc,json);req->send(200,"application/json",json);
  });

  // Layout (2D oder)
  server->on("/api/layouts",HTTP_GET,apiGetLayouts);
  server->on("/api/layout",HTTP_GET,apiGetLayout);
  server->on("/api/layout",HTTP_POST,[](AsyncWebServerRequest* req){},NULL,apiLayoutSave);
  server->on("/api/layoutdelete",HTTP_GET,apiLayoutDelete);

  // Config management
  server->on("/api/cfglist",HTTP_GET,apiGetCfgList);
  server->on("/api/cfgsave",HTTP_POST,[](AsyncWebServerRequest* req){},NULL,apiCfgSave);
  server->on("/api/cfgload",HTTP_POST,[](AsyncWebServerRequest* req){},NULL,apiCfgLoad);
  server->on("/api/cfgdelete",HTTP_POST,[](AsyncWebServerRequest* req){},NULL,apiCfgDelete);
  server->on("/api/cfgdownload",HTTP_GET,apiCfgDownload);
  server->on("/api/cfgupload",HTTP_POST,[](AsyncWebServerRequest* req){},apiCfgUpload);

  // OTA firmware update
  server->on("/api/update", HTTP_POST,
    [](AsyncWebServerRequest* req) {
      bool ok = !Update.hasError();
      req->send(200, "application/json", ok ? "{\"ok\":true}" : "{\"ok\":false,\"error\":\"Update failed\"}");
      if (ok) { delay(500); ESP.restart(); }
    },
    [](AsyncWebServerRequest* req, const String& filename, size_t index, uint8_t* data, size_t len, bool final) {
      if (index == 0) {
        Serial.printf("[OTA] Start: %s\n", filename.c_str());
        if (!Update.begin(UPDATE_SIZE_UNKNOWN)) Update.printError(Serial);
      }
      if (Update.isRunning()) {
        if (Update.write(data, len) != len) Update.printError(Serial);
      }
      if (final) {
        if (Update.end(true)) Serial.printf("[OTA] OK, %u bytes\n", index + len);
        else Update.printError(Serial);
      }
    }
  );

  server->begin();
  Serial.println("[WEB] AsyncWebServer + WebSocket zagnan (6 zavihkov)");
}

void webLoop() {
  if(!_ws||!_mix)return;

  // ArtNet detected v PRIMARY načinu — pošlji notifikacijo (max 1x/30s)
  static unsigned long lastArtnetNotif = 0;
  if (_mix->consumeArtNetDetected()) {
    unsigned long nowMs = millis();
    if (nowMs - lastArtnetNotif > 30000) {
      lastArtnetNotif = nowMs;
      if (_ws->count() > 0) _ws->textAll("{\"t\":\"artnet_detected\"}");
    }
  }

  unsigned long now=millis();
  if(!_forceSendState && now-_lastWsSend<WS_UPDATE_INTERVAL)return;
  _lastWsSend=now; _forceSendState=false;
  if(_ws->count()==0)return;

  JsonDocument doc;
  doc["t"]="status"; doc["mode"]=(int)_mix->getMode(); doc["fps"]=_mix->getArtNetFps();
  doc["pkts"]=_mix->getArtNetPackets(); doc["master"]=_mix->getMasterDimmer(); doc["bo"]=_mix->isBlackout(); doc["flash"]=_mix->isFlashing();
  doc["heap"]=esp_get_free_heap_size();
  doc["iheap"]=heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
  doc["mspd"]=_mix->getMasterSpeed();

  // Group dimmers
  JsonArray gd=doc["gd"].to<JsonArray>();
  for(int g=0;g<MAX_GROUPS;g++) gd.add(_mix->getGroupDimmer(g));

  // Crossfade
  JsonObject cf=doc["cf"].to<JsonObject>();
  cf["active"]=_mix->isSceneCrossfading(); cf["progress"]=_mix->getSceneCrossfadeProgress();
  cf["target"]=_mix->getSceneCrossfadeTarget();

  // Locate
  uint32_t locMask=_mix->getLocateMask();
  if(locMask) doc["loc"]=locMask;

  // Cue list status
  if(_scn){
    _scn->cueUpdateAutoFollow(_mix);
    JsonObject cl=doc["cl"].to<JsonObject>();
    cl["cur"]=_scn->getCurrentCue();cl["cnt"]=_scn->getCueCount();cl["run"]=_scn->isCueRunning();
  }

  // LFO status
  if(_lfo && _lfo->isActive()){
    JsonArray la=doc["lfos"].to<JsonArray>();
    for(int i=0;i<MAX_LFOS;i++){
      const LfoInstance* l=_lfo->getLfo(i);
      if(!l||!l->active){la.add(nullptr);continue;}
      JsonObject o=la.add<JsonObject>();
      o["w"]=l->waveform;o["tgt"]=l->target;o["rate"]=serialized(String(l->rate,2));
      o["depth"]=serialized(String(l->depth,2));o["phase"]=serialized(String(l->phase,2));
      o["mask"]=l->fixtureMask;o["sym"]=l->symmetry;o["p"]=serialized(String(l->currentPhase,2));
    }
  }

  // Shape status
  if(_shapeGen && _shapeGen->isActive()){
    JsonArray sa=doc["shapes"].to<JsonArray>();
    for(int i=0;i<MAX_SHAPES;i++){
      const ShapeInstance* s=_shapeGen->getShape(i);
      if(!s||!s->active){sa.add(nullptr);continue;}
      JsonObject o=sa.add<JsonObject>();
      o["type"]=s->type;o["rate"]=serialized(String(s->rate,2));
      o["sx"]=serialized(String(s->sizeX,2));o["sy"]=serialized(String(s->sizeY,2));
      o["phase"]=serialized(String(s->phase,2));o["mask"]=s->fixtureMask;
    }
  }

  // FFT
  if(_snd&&_aud&&_aud->isRunning()){
    const FFTBands& bands=_snd->getBands();
    JsonObject fft=doc["fft"].to<JsonObject>();
    JsonArray b=fft["b"].to<JsonArray>();
    for(int i=0;i<STL_BAND_COUNT;i++) b.add(bands.bands[i]);
    fft["bass"]=bands.bass; fft["mid"]=bands.mid; fft["high"]=bands.high;
    fft["beat"]=bands.beatDetected; fft["bpm"]=bands.bpm;
    fft["bp"]=_snd->getBeatPhase();
    fft["peak"]=_aud->getPeakLevel(); fft["sr"]=_aud->getSampleRate();
    // AGC config za UI sinhronizacijo
    const STLAgcConfig& agc=_snd->getAgcConfig();
    fft["aspd"]=agc.agcSpeed; fft["ang"]=agc.noiseGate;
    JsonArray abg=fft["abg"].to<JsonArray>();
    for(int i=0;i<STL_BAND_COUNT;i++) abg.add(agc.bandGains[i]);
    // Fixture sound levels za preview
    if(_snd->isActive()){
      JsonArray fxl=doc["fxl"].to<JsonArray>();
      for(int i=0;i<MAX_FIXTURES;i++) fxl.add((int)(_snd->getFixtureLevel(i)*100));
    }
  }

  // Manual beat
  if(_snd){
    JsonObject mb=doc["mb"].to<JsonObject>();
    const ManualBeatConfig& mbc=_snd->getManualBeatConfig();
    mb["en"]=mbc.enabled;
    mb["bpm"]=mbc.bpm;
    mb["prog"]=mbc.program;
    mb["sub"]=mbc.subdiv;
    mb["src"]=mbc.source;
    mb["phase"]=_snd->getBeatPhase();
    mb["active"]=_snd->isManualBeatActive();
    mb["taps"]=_snd->getTapCount();
    // Ableton Link status
    const LinkBeat& lnk = _snd->getLinkBeat();
    if (lnk.isEnabled()) {
      mb["link_on"]=true; mb["link_peers"]=lnk.getPeerCount();
      mb["link_bpm"]=lnk.getBpm(); mb["link_conn"]=lnk.isConnected();
    }
    // Faza 3: krivulje + envelope
    mb["dc"]=mbc.dimCurve;
    mb["atk"]=mbc.attackMs;
    mb["dcy"]=mbc.decayMs;
    // FX simetrija
    mb["sym"]=mbc.symmetry;
    // Faza 4: paleta
    mb["pal"]=mbc.palette;
    // Faza 5: chain status
    if(_snd->getChain().active){
      JsonObject ch=mb["chain"].to<JsonObject>();
      ch["idx"]=_snd->getChainIdx();
      ch["cnt"]=_snd->getChain().count;
    }
    // Faza 6: per-group overrides (samo aktivne)
    bool hasGrpOv=false;
    for(int g=0;g<MAX_GROUPS;g++){
      if(mbc.groupOverrides[g].program!=GROUP_BEAT_INHERIT){hasGrpOv=true;break;}
    }
    if(hasGrpOv){
      JsonArray ga=mb["grp"].to<JsonArray>();
      for(int g=0;g<MAX_GROUPS;g++){
        const GroupBeatOverride& ov=mbc.groupOverrides[g];
        JsonObject go=ga.add<JsonObject>();
        go["p"]=ov.program;go["s"]=ov.subdiv;go["i"]=ov.intensity;
      }
    }
    // Fixture sound levels za preview (tudi iz manual beat)
    if(_snd->isActive()&&!doc.containsKey("fxl")){
      JsonArray fxl=doc["fxl"].to<JsonArray>();
      for(int i=0;i<MAX_FIXTURES;i++) fxl.add((int)(_snd->getFixtureLevel(i)*100));
    }
  }

  // Undo
  doc["undo"]=_mix->hasUndo();

  // Snapshots
  JsonArray snaps=doc["snaps"].to<JsonArray>();
  for(int i=0;i<_mix->getSnapshotCount();i++){
    const StateSnapshot* s=_mix->getSnapshot(i);
    if(s&&s->valid){JsonObject o=snaps.add<JsonObject>();o["ts"]=s->timestamp;char src[2]={s->source,0};o["src"]=src;}
  }

  // Fixture vrednosti za sinhronizacijo sliderjev
  // V ArtNet načinu prikaži dejanski ArtNet vhod, v lokalnem pa ročne vrednosti
  const uint8_t* vals = (_mix->getMode() == CTRL_ARTNET) ? _mix->getDmxOutput() : _mix->getManualValues();
  JsonArray fxv=doc["fxv"].to<JsonArray>();
  for(int i=0;i<MAX_FIXTURES;i++){
    const PatchEntry* fx=_fix->getFixture(i);
    if(!fx||!fx->active){fxv.add(nullptr);continue;}
    JsonArray chv=fxv.add<JsonArray>();
    uint8_t chCount=_fix->fixtureChannelCount(i);
    for(int c=0;c<chCount;c++){
      uint16_t addr=fx->dmxAddress+c-1;
      chv.add(addr<DMX_MAX_CHANNELS?vals[addr]:0);
    }
  }

  // Fixture output vrednosti za prikaz (dejanski DMX izhod z beatom, master/group dimmerji)
  const uint8_t* outVals=_mix->getDmxOutput();
  JsonArray fxo=doc["fxo"].to<JsonArray>();
  for(int i=0;i<MAX_FIXTURES;i++){
    const PatchEntry* fx=_fix->getFixture(i);
    if(!fx||!fx->active){fxo.add(nullptr);continue;}
    JsonArray cho=fxo.add<JsonArray>();
    uint8_t chCount=_fix->fixtureChannelCount(i);
    for(int c=0;c<chCount;c++){
      uint16_t addr=fx->dmxAddress+c-1;
      cho.add(addr<DMX_MAX_CHANNELS?outVals[addr]:0);
    }
  }

  String json; serializeJson(doc,json); _ws->textAll(json); _ws->cleanupClients();

  // DMX Monitor — send raw 512 bytes as base64 (only when active)
  if (_dmxMonActive && _ws->count() > 0) {
    String b64 = dmxToBase64(_mix->getDmxOutput());
    _ws->textAll("{\"t\":\"dmx\",\"d\":\"" + b64 + "\"}");
  }
}
