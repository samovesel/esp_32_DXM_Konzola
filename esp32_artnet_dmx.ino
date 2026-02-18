// ============================================================================
//  ESP32 ArtNet/DMX Node z mešalno mizo — Kompletna verzija
//  Verzija: 1.0.0
//
//  Funkcionalnosti:
//  - ArtNet → DMX izhod (1 univerza, 512 kanalov)
//  - State machine: ARTNET / LOCAL_AUTO / LOCAL_MANUAL
//  - Fixture profili (JSON na LittleFS)
//  - Fixture patch, skupine
//  - Spletni vmesnik s faderji, fixture managementom
//  - WebSocket real-time komunikacija
//  - Zgodovina stanj (5 snapshot-ov)
//  - Scene: shrani/recall s crossfade (do 20 scen, 0-10s fade)
//  - Sound-to-light: FFT analiza, easy/pro mode, beat detection
//  - Audio vhod: ADC line-in ali I2S MEMS mikrofon (INMP441)
//  - RGB status LED
//
//  Odvisnosti (Arduino Library Manager):
//  - ArtnetWifi by Nathanaël Lécaudé
//  - ESPAsyncWebServer (mathieucarbou fork za ESP32 core 3.x)
//  - AsyncTCP (mathieucarbou fork)
//  - ArduinoJson 7.x
//  - arduinoFFT by Enrique Condes
//  - Adafruit_NeoPixel (samo za ESP32-S3 — vgrajena WS2812 LED)
//  - LittleFS (vgrajen v ESP32 core)
//
//  Arduino IDE nastavitve:
//
//  ESP32-S3 N16R8:
//  - Board: ESP32S3 Dev Module
//  - USB CDC On Boot: Enabled
//  - Flash Size: 16MB (128Mb)
//  - Flash Mode: QIO 80MHz
//  - PSRAM: OPI PSRAM
//  - Partition Scheme: Custom (partitions.csv v mapi sketcha)
//  - CPU Frequency: 240MHz
//
//  ESP32 DevKit V1:
//  - Board: ESP32 Dev Module
//  - Partition Scheme: "No OTA (2MB APP/2MB SPIFFS)" ali "Huge APP"
//  - POMEMBNO: IZKLOPI Bluetooth (prihranek ~70KB DRAM)
// ============================================================================

#include <WiFi.h>
#include <WiFiUdp.h>
#include <ArtnetWifi.h>
#include <LittleFS.h>
#include "esp_bt.h"
#include "esp_task_wdt.h"

#include "config.h"
#include "config_store.h"
#include "dmx_driver.h"
#include "fixture_engine.h"
#include "mixer_engine.h"
#include "scene_engine.h"
#include "audio_input.h"
#include "sound_engine.h"
#include "led_status.h"
#include "web_ui.h"

// ============================================================================
//  GLOBALNI OBJEKTI
// ============================================================================

NodeConfig     nodeCfg;
DmxOutput      dmxOut;
FixtureEngine  fixtures;
MixerEngine    mixer;
SceneEngine    scenes;
AudioInput     audioIn;
SoundEngine    soundEng;
ArtnetWifi     artnet;

AsyncWebServer webServer(80);
AsyncWebSocket webSocket("/ws");
WiFiUDP        pollReplyUdp;   // Za ArtPollReply broadcast

// DMX output timing
static unsigned long lastDmxSend = 0;
const unsigned long  DMX_INTERVAL_US = 25000;  // 40 fps DMX izhod

// LED blink
static unsigned long lastLedUpdate = 0;
static bool ledBlink = false;

// ArtPollReply periodic broadcast
static unsigned long lastPollReply = 0;
const unsigned long  POLL_REPLY_INTERVAL = 5000;  // Vsake 5s

// Watchdog
#define WDT_TIMEOUT_S       8      // Sekund do reseta
#define SAFE_MODE_THRESHOLD 3      // Število WDT resetov pred safe modo

// Safe mode: po 3 zaporednih WDT resetih se požene brez sound FFT
static RTC_NOINIT_ATTR uint32_t wdtResetCount;  // Preživi soft-reset (v RTC memory)
static bool safeMode = false;

// Forward declaration za dual-core task
void auxTask(void* param);

#include <ESPmDNS.h>

// ============================================================================
//  WIFI — Multi-AP failover
// ============================================================================

void wifiSetup() {
  WiFi.setHostname(nodeCfg.hostname);

  // Poišči prvi konfiguriran AP
  int configuredCount = 0;
  for (int i = 0; i < MAX_WIFI_APS; i++) {
    if (nodeCfg.wifiAPs[i].ssid[0] != '\0') configuredCount++;
  }

  if (configuredCount > 0) {
    WiFi.mode(WIFI_STA);

    if (!nodeCfg.dhcp) {
      IPAddress ip, gw, sn;
      ip.fromString(nodeCfg.staticIp);
      gw.fromString(nodeCfg.staticGw);
      sn.fromString(nodeCfg.staticSn);
      WiFi.config(ip, gw, sn);
    }

    bool connected = false;
    for (int ap = 0; ap < MAX_WIFI_APS && !connected; ap++) {
      if (nodeCfg.wifiAPs[ap].ssid[0] == '\0') continue;

      Serial.printf("[WiFi] Poskušam AP %d: '%s'...\n", ap + 1, nodeCfg.wifiAPs[ap].ssid);
      WiFi.begin(nodeCfg.wifiAPs[ap].ssid, nodeCfg.wifiAPs[ap].password);

      int tries = 0;
      while (WiFi.status() != WL_CONNECTED && tries < 20) {
        delay(500);
        Serial.print(".");
        ledSet((tries % 2) ? LED_BLUE : LED_OFF);
        tries++;
      }

      if (WiFi.status() == WL_CONNECTED) {
        connected = true;
        Serial.printf("\n[WiFi] Povezan na '%s'! IP: %s\n",
                      nodeCfg.wifiAPs[ap].ssid, WiFi.localIP().toString().c_str());
        ledSet(LED_GREEN);
      } else {
        Serial.printf("\n[WiFi] '%s' ni uspelo.\n", nodeCfg.wifiAPs[ap].ssid);
        WiFi.disconnect();
      }
    }

    if (!connected) {
      Serial.println("[WiFi] Noben AP ni uspel → AP mode");
      WiFi.mode(WIFI_AP);
      WiFi.softAP(nodeCfg.hostname, "artnetdmx");
      Serial.printf("[WiFi] AP '%s', IP: %s\n", nodeCfg.hostname, WiFi.softAPIP().toString().c_str());
      ledSet(LED_YELLOW);
    }
  } else {
    // AP mode (brez konfiguriranih SSID-jev)
    WiFi.mode(WIFI_AP);
    WiFi.softAP(nodeCfg.hostname, "artnetdmx");
    Serial.printf("[WiFi] AP '%s', IP: %s\n", nodeCfg.hostname, WiFi.softAPIP().toString().c_str());
    ledSet(LED_YELLOW);
  }

  // mDNS — dostop prek hostname.local (npr. artnet-node.local)
  if (MDNS.begin(nodeCfg.hostname)) {
    MDNS.addService("artnet", "udp", 6454);
    MDNS.addService("http", "tcp", 80);
    Serial.printf("[mDNS] http://%s.local\n", nodeCfg.hostname);
  }
}

// ============================================================================
//  ARTNET CALLBACK
// ============================================================================

void onArtNetDmx(uint16_t universe, uint16_t length, uint8_t sequence, uint8_t* data) {
  if (universe == nodeCfg.universe) {
    mixer.onArtNetData(data, length);
  }
}

// ============================================================================
//  ARTPOLL REPLY — ročna implementacija (ArtnetWifi jo ne pošlje pravilno)
// ============================================================================

void sendArtPollReply() {
  uint8_t reply[239];
  memset(reply, 0, sizeof(reply));

  // Art-Net header
  memcpy(reply, "Art-Net\0", 8);
  reply[8] = 0x00; reply[9] = 0x21;  // OpPollReply (0x2100) little-endian

  // IP naslov
  IPAddress ip = WiFi.getMode() == WIFI_AP ? WiFi.softAPIP() : WiFi.localIP();
  reply[10] = ip[0]; reply[11] = ip[1]; reply[12] = ip[2]; reply[13] = ip[3];

  // Port (little-endian)
  reply[14] = 0x36; reply[15] = 0x19;  // 6454

  // Firmware version
  reply[16] = 0; reply[17] = 1;

  // NetSwitch, SubSwitch
  reply[18] = (nodeCfg.universe >> 8) & 0x7F;  // Net (bits 14-8)
  reply[19] = (nodeCfg.universe >> 4) & 0x0F;  // Sub-Net (bits 7-4)

  // OEM code
  reply[20] = 0xFF; reply[21] = 0xFF;

  // UBEA version, Status1
  reply[22] = 0; reply[23] = 0x30;  // Normal mode, indicators normal

  // ESTA manufacturer
  reply[24] = 0; reply[25] = 0;

  // Short name (max 18 chars)
  strncpy((char*)&reply[26], nodeCfg.hostname, 17);
  reply[43] = 0;

  // Long name (max 64 chars)
  char longName[64];
  snprintf(longName, sizeof(longName), "ESP32 ArtNet/DMX Node - %s", nodeCfg.hostname);
  strncpy((char*)&reply[44], longName, 63);
  reply[107] = 0;

  // Node report (max 64 chars)
  char report[64];
  snprintf(report, sizeof(report), "#0001 [%04d] OK - Universe %d", (int)millis()/1000, nodeCfg.universe);
  strncpy((char*)&reply[108], report, 63);
  reply[171] = 0;

  // NumPorts (hi, lo)
  reply[172] = 0; reply[173] = 1;  // 1 output port

  // Port types: DMX512 output
  reply[174] = 0x80;  // Port 0 = output, protocol = DMX512

  // Good input
  reply[178] = 0x08;  // Data received

  // Good output
  reply[182] = 0x80;  // Data being transmitted

  // SwIn (input universe for each port)
  reply[186] = nodeCfg.universe & 0x0F;

  // SwOut (output universe for each port)
  reply[190] = nodeCfg.universe & 0x0F;

  // Style: StNode
  reply[200] = 0x00;  // StNode

  // MAC address
  uint8_t mac[6];
  WiFi.macAddress(mac);
  reply[201] = mac[0]; reply[202] = mac[1]; reply[203] = mac[2];
  reply[204] = mac[3]; reply[205] = mac[4]; reply[206] = mac[5];

  // Bind IP (same as node IP)
  reply[207] = ip[0]; reply[208] = ip[1]; reply[209] = ip[2]; reply[210] = ip[3];

  // Bind index
  reply[211] = 1;

  // Status2: supports Art-Net 3, DHCP capable
  reply[212] = 0x0E;

  // Pošlji kot broadcast na port 6454
  IPAddress subnet = WiFi.getMode() == WIFI_AP ? IPAddress(255,255,255,0) : WiFi.subnetMask();
  IPAddress broadcast;
  for (int i = 0; i < 4; i++) broadcast[i] = ip[i] | ~subnet[i];

  pollReplyUdp.beginPacket(broadcast, 6454);
  pollReplyUdp.write(reply, sizeof(reply));
  pollReplyUdp.endPacket();
}

// ============================================================================
//  FACTORY RESET CHECK
// ============================================================================

bool checkFactoryReset() {
  pinMode(RESET_BTN_PIN, INPUT_PULLUP);
  delay(50);
  if (digitalRead(RESET_BTN_PIN) == LOW) {
    Serial.println("[SYS] Reset gumb držan — čakam 3s...");
    ledSet(LED_RED);
    delay(3000);
    if (digitalRead(RESET_BTN_PIN) == LOW) {
      Serial.println("[SYS] TOVARNIŠKA PONASTAVITEV!");
      LittleFS.format();
      ledSet(LED_WHITE);
      delay(1000);
      ESP.restart();
      return true;
    }
  }
  return false;
}

// ============================================================================
//  PRIVZETI PROFILI — zapišejo se v LittleFS ob prvem zagonu
// ============================================================================

static const char PROF_PAR_RGBW[] PROGMEM = R"({"name":"LED PAR RGBW 7ch","channels":[{"name":"Dimmer","type":"intensity","default":0},{"name":"Red","type":"color_r","default":0},{"name":"Green","type":"color_g","default":0},{"name":"Blue","type":"color_b","default":0},{"name":"White","type":"color_w","default":0},{"name":"Strobe","type":"strobe","default":0},{"name":"Preset","type":"preset","default":0}]})";

static const char PROF_PAR_RGBLAC[] PROGMEM = R"({"name":"PAR RGBLAC 12ch","channels":[{"name":"Dimmer","type":"intensity","default":0},{"name":"Red","type":"color_r","default":0},{"name":"Green","type":"color_g","default":0},{"name":"Blue","type":"color_b","default":0},{"name":"Lime","type":"color_a","default":0},{"name":"Amber","type":"color_a","default":0},{"name":"Cyan","type":"color_b","default":0},{"name":"UV","type":"color_uv","default":0},{"name":"Strobe","type":"strobe","default":0},{"name":"Color Macro","type":"macro","default":0},{"name":"Auto","type":"preset","default":0},{"name":"Speed","type":"speed","default":128}]})";

static const char PROF_MH_16CH[] PROGMEM = R"({"name":"Moving Head 16ch","channels":[{"name":"Pan","type":"pan","default":128},{"name":"Pan fine","type":"pan_fine","default":0},{"name":"Tilt","type":"tilt","default":128},{"name":"Tilt fine","type":"tilt_fine","default":0},{"name":"Speed","type":"speed","default":0},{"name":"Dimmer","type":"intensity","default":0},{"name":"Red","type":"color_r","default":0},{"name":"Green","type":"color_g","default":0},{"name":"Blue","type":"color_b","default":0},{"name":"White","type":"color_w","default":0},{"name":"Strobe","type":"strobe","default":0},{"name":"Gobo","type":"gobo","default":0},{"name":"Prism","type":"prism","default":0},{"name":"Focus","type":"focus","default":128},{"name":"Zoom","type":"zoom","default":128},{"name":"Preset","type":"preset","default":0}]})";

struct DefaultProfile { const char* filename; const char* data; };

static const DefaultProfile DEF_PROFILES[] = {
  { "/profiles/par-rgbw-7ch.json",    PROF_PAR_RGBW },
  { "/profiles/par-rgblac-12ch.json", PROF_PAR_RGBLAC },
  { "/profiles/mh-16ch.json",         PROF_MH_16CH }
};

void installDefaultProfiles() {
  if (!LittleFS.exists("/profiles")) LittleFS.mkdir("/profiles");
  if (!LittleFS.exists("/scenes"))   LittleFS.mkdir("/scenes");

  int n = 0;
  for (int i = 0; i < 3; i++) {
    if (!LittleFS.exists(DEF_PROFILES[i].filename)) {
      File f = LittleFS.open(DEF_PROFILES[i].filename, "w");
      if (f) { f.print(DEF_PROFILES[i].data); f.close(); n++; }
    }
  }
  if (n) Serial.printf("[SYS] Nameščenih %d privzetih profilov\n", n);
}

// ============================================================================
//  SETUP
// ============================================================================

void setup() {
  // Sprosti BT pomnilnik — ne rabimo Bluetooth-a
#if defined(CONFIG_IDF_TARGET_ESP32S3)
  esp_bt_controller_mem_release(ESP_BT_MODE_BLE);     // S3 ima samo BLE
#else
  esp_bt_controller_mem_release(ESP_BT_MODE_BTDM);    // ESP32: BT+BLE (~64KB DRAM)
#endif

  Serial.begin(115200);
  delay(200);
  Serial.println("\n====================================");
#if defined(CONFIG_IDF_TARGET_ESP32S3)
  Serial.println("  ESP32-S3 ArtNet/DMX Node " FW_VERSION);
#else
  Serial.println("  ESP32 ArtNet/DMX Node " FW_VERSION);
#endif
  Serial.println("====================================\n");

#if HAS_PSRAM
  if (psramFound()) {
    Serial.printf("[SYS] PSRAM: %d KB\n", ESP.getPsramSize() / 1024);
  } else {
    Serial.println("[SYS] OPOZORILO: PSRAM ni zaznan!");
  }
#endif

  // --- Watchdog: preveri razlog zadnjega reseta ---
  esp_reset_reason_t reason = esp_reset_reason();
  if (reason == ESP_RST_TASK_WDT || reason == ESP_RST_WDT || reason == ESP_RST_PANIC) {
    wdtResetCount++;
    Serial.printf("[WDT] Nenormalen reset (#%d), razlog: %d\n", wdtResetCount, (int)reason);
  } else {
    wdtResetCount = 0;  // Normalen zagon — ponastavi števec
  }
  if (wdtResetCount >= SAFE_MODE_THRESHOLD) {
    safeMode = true;
    Serial.println("[WDT] === SAFE MODE === Po več crashih zaganjam brez zvoka/FFT");
  }

  // LED
  ledBegin();
  ledSet(safeMode ? LED_RED : LED_BLUE);

  // Factory reset check
  checkFactoryReset();

  // LittleFS
  if (!LittleFS.begin(true)) {
    Serial.println("[SYS] LittleFS NAPAKA! Formatiram...");
    LittleFS.format();
    LittleFS.begin(true);
  }
  Serial.printf("[SYS] LittleFS: %d KB used / %d KB total\n",
                LittleFS.usedBytes() / 1024, LittleFS.totalBytes() / 1024);

  // Privzeti profili (zapišejo se ob prvem zagonu)
  installDefaultProfiles();

  // Konfiguracija
  configLoad(nodeCfg);
  Serial.printf("[CFG] Hostname: %s, Universe: %d, Channels: %d\n",
                nodeCfg.hostname, nodeCfg.universe, nodeCfg.channelCount);

  // WiFi
  wifiSetup();

  // DMX output
  dmxOut.begin(DMX_TX_PIN, DMX_RX_PIN, DMX_DE_PIN);

  // Fixture engine
  fixtures.begin();

  // Scene engine
  scenes.begin();

  // Mixer
  mixer.begin(&fixtures, &scenes);
  mixer.setArtNetTimeout(nodeCfg.artnetTimeoutSec * 1000UL);
  if (nodeCfg.artnetPrimaryMode) mixer.switchToPrimaryLocal();

  // Sound engine (preskoči v safe mode)
  if (!safeMode) {
    soundEng.begin(&audioIn, &fixtures);
    mixer.setSoundEngine(&soundEng);

    // Audio vhod (če je konfiguriran)
    if (nodeCfg.audioSource > 0) {
      if (audioIn.begin(nodeCfg.audioSource)) {
        Serial.printf("[AUD] Audio vhod aktiven: %s\n",
                      nodeCfg.audioSource == 1 ? "ADC line-in" : "I2S mikrofon");
      }
    } else {
      Serial.println("[AUD] Audio vhod izklopljen");
    }
  } else {
    Serial.println("[AUD] Preskoči zvok (safe mode)");
  }

  // ArtNet
  artnet.begin();
  artnet.setArtDmxCallback(onArtNetDmx);
  pollReplyUdp.begin(0);

  // Web server
  webBegin(&webServer, &webSocket, &nodeCfg, &fixtures, &mixer, &scenes, &soundEng, &audioIn);

  // Pošlji ArtPollReply takoj ob zagonu
  sendArtPollReply();
  lastPollReply = millis();

  // --- Watchdog: rekonfiguriraj obstoječi TWDT (Arduino core ga že inicializira) ---
  const esp_task_wdt_config_t wdtCfg = {
    .timeout_ms = WDT_TIMEOUT_S * 1000,
    .idle_core_mask = 0,
    .trigger_panic = true
  };
  esp_task_wdt_reconfigure(&wdtCfg);
  esp_task_wdt_add(NULL);  // Dodaj loopTask (core 1)

  // --- Ustvari pomožni task na core 0 ---
  xTaskCreatePinnedToCore(
    auxTask,       // Funkcija
    "AUX",         // Ime
    HAS_PSRAM ? 8192 : 4096,  // Stack (8KB s PSRAM, 4KB brez)
    NULL,          // Parameter
    2,             // Prioriteta (nižja od loopTask ki je 1... hmm)
    NULL,          // Handle
    0              // Core 0 (deli z WiFi)
  );

  Serial.printf("[SYS] Dual-core: DMX+Web na core 1, Sound+LED na core 0%s\n",
                safeMode ? " (SAFE MODE)" : "");
  Serial.printf("[SYS] Prosti heap: %d B, min: %d B\n",
                esp_get_free_heap_size(), esp_get_minimum_free_heap_size());
  Serial.println("[SYS] Inicializacija končana. Pripravljen.\n");
}

// ============================================================================
//  CORE 1: DMX REALTIME LOOP (Arduino loop() — visoka prioriteta)
//  ArtNet branje → Mixer update → DMX output
//  Mora biti hitro in deterministično (< 2ms/iteracijo)
// ============================================================================

void loop() {
  // Watchdog feed
  esp_task_wdt_reset();

  // ArtNet — branje UDP paketov
  artnet.read();

  // Mixer update — timeout logika, sestavi izhod (+ sound overlay)
  mixer.update();

  // DMX output — pošlji okvir s konstantnim intervalom
  unsigned long now = micros();
  if (now - lastDmxSend >= DMX_INTERVAL_US) {
    lastDmxSend = now;
    dmxOut.sendFrame(mixer.getDmxOutput(), nodeCfg.channelCount);
  }

  // Web — periodično pošiljanje stanja prek WebSocket (mora biti na core 1, ker AsyncTCP ni thread-safe)
  webLoop();

  // ArtPollReply — periodični broadcast za discovery
  unsigned long nowMs = millis();
  if (nowMs - lastPollReply >= POLL_REPLY_INTERVAL) {
    lastPollReply = nowMs;
    sendArtPollReply();
  }

  vTaskDelay(1);  // 1 tick = 1ms
}

// ============================================================================
//  CORE 0: POMOŽNI TASK
//  Sound FFT in LED status — deli core z WiFi/AsyncTCP stackom
// ============================================================================

void auxTask(void* param) {
  esp_task_wdt_add(NULL);

  for (;;) {
    esp_task_wdt_reset();

    // Sound engine — obdela FFT če so novi vzorci
    if (!safeMode) {
      soundEng.update();
    }

    // LED status
    unsigned long nowMs = millis();
    if (nowMs - lastLedUpdate > 500) {
      lastLedUpdate = nowMs;
      ledBlink = !ledBlink;

      if (safeMode) {
        ledSet(ledBlink ? LED_RED : LED_OFF);
      } else {
        switch (mixer.getMode()) {
          case CTRL_ARTNET:
            ledSet(mixer.getArtNetFps() > 0 ? (ledBlink ? LED_GREEN : LED_OFF) : LED_GREEN);
            break;
          case CTRL_LOCAL_AUTO:
            ledSet(LED_CYAN);
            break;
          case CTRL_LOCAL_MANUAL:
            ledSet(LED_PURPLE);
            break;
          case CTRL_LOCAL_PRIMARY:
            ledSet(LED_BLUE);
            break;
        }
      }
    }

    vTaskDelay(2);  // 2ms — sound FFT ne rabi hitreje
  }
}
