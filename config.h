#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

// ============================================================================
//  VERZIJA
// ============================================================================
#define FW_VERSION "1.0.0"

// ============================================================================
//  OMEJITVE
// ============================================================================
#define MAX_FIXTURES        24
#define MAX_PROFILES        16    // 14 fixturov + 2 rezerva; vsak fixture 1 aktiven mode
#define MAX_CHANNELS_PER_FX 24    // Pokrije 19ch moving heade in segmentirane naprave
#define MAX_RANGES_PER_CH    6    // Pokrije OFL importe; grupiraj če > 6
#define MAX_GROUPS           8
#define MAX_STATE_SNAPSHOTS  3    // Zmanjšano iz 5 — prihranek ~1KB
#define MAX_SCENES          20
#define MAX_SCENE_NAME_LEN  24
#define MAX_CUES            40
#define MAX_CONFIGS          8    // Shranjene konfiguracije na LittleFS
#define CROSSFADE_MIN_MS    0      // Takojšen
#define CROSSFADE_MAX_MS    10000  // 10 sekund
#define CROSSFADE_DEFAULT_MS 1500  // 1.5 sekunde
#define DMX_MAX_CHANNELS   512

// ============================================================================
//  GPIO DODELITVE + PLATFORMA
// ============================================================================
#if defined(CONFIG_IDF_TARGET_ESP32S3)
// --- ESP32-S3 N16R8 ---
// GPIO 26-37 NI NA VOLJO (Octal SPI flash/PSRAM)
// Na voljo: 0-21, 38-48

#define DMX_TX_PIN         17    // UART1 TX → MAX485 DI
#define DMX_RX_PIN         18    // UART1 RX (neuporabljen pri TX)
#define DMX_DE_PIN          8    // MAX485 DE+RE
#define DMX_UART_PORT      UART_NUM_1

#define NEOPIXEL_PIN       48    // Vgrajena WS2812 NeoPixel
#define NEOPIXEL_COUNT      1

#define RESET_BTN_PIN      14    // Drži LOW ob zagonu = factory reset

// Audio — WM8782S I2S ADC (line-in, ESP32 slave)
#define I2S_LINE_BCK_PIN    4    // WM8782S BCLK
#define I2S_LINE_WS_PIN     5    // WM8782S LRCK
#define I2S_LINE_DATA_PIN   6    // WM8782S DOUT
// Audio — INMP441 I2S MEMS mikrofon (ESP32 master)
#define I2S_MIC_BCK_PIN    12    // INMP441 BCLK
#define I2S_MIC_WS_PIN     11    // INMP441 WS
#define I2S_MIC_DATA_PIN   10    // INMP441 DATA

#define HAS_PSRAM           1

#else
// --- ESP32 DevKit V1 (original) ---
#define DMX_TX_PIN         17    // UART2 TX → MAX485 DI
#define DMX_RX_PIN         16    // UART2 RX (neuporabljen pri TX)
#define DMX_DE_PIN          4    // MAX485 DE+RE
#define DMX_UART_PORT      UART_NUM_2

#define LED_R_PIN          25
#define LED_G_PIN          26
#define LED_B_PIN          27

#define RESET_BTN_PIN      14    // Drži LOW ob zagonu = factory reset

// Audio — WM8782S I2S ADC (line-in, ESP32 slave)
#define I2S_LINE_BCK_PIN   32    // WM8782S BCLK
#define I2S_LINE_WS_PIN    33    // WM8782S LRCK
#define I2S_LINE_DATA_PIN  36    // WM8782S DOUT (input-only GPIO)
// Audio — INMP441 I2S MEMS mikrofon (ESP32 master)
#define I2S_MIC_BCK_PIN    22    // INMP441 BCLK
#define I2S_MIC_WS_PIN     21    // INMP441 WS
#define I2S_MIC_DATA_PIN   34    // INMP441 DATA

#define HAS_PSRAM           0
#endif

// Sound-to-light (HAS_PSRAM mora biti definiran prej!)
#if HAS_PSRAM
#define FFT_SAMPLES        1024    // Boljša frekvenčna ločljivost s PSRAM (~10.7 Hz/bin)
#else
#define FFT_SAMPLES        512     // Konservativno za ESP32 DRAM (~21.5 Hz/bin)
#endif
#define FFT_SAMPLE_RATE    22050   // Hz — dovolj za do 11kHz analizo
#define WM8782S_SAMPLE_RATE 96000  // WM8782S master mode z 24.576MHz MCLK (jumper 96K)
#define WM8782S_DECIMATION  4      // 96kHz / 4 = 24kHz (blizu FFT_SAMPLE_RATE)
#define FFT_BINS           (FFT_SAMPLES / 2)
#define STL_MAX_RULES      8      // Zmanjšano iz 16
#define STL_BAND_COUNT     8      // Število frekvenčnih pasov za vizualizacijo
#define STL_ATTACK_DEFAULT 50     // ms
#define STL_DECAY_DEFAULT  200    // ms
#define BEAT_HISTORY_SIZE  24     // Zmanjšano iz 32

// AGC (Automatic Gain Control) za sound-to-light
#define AGC_DECAY_SLOW     0.999f  // Počasen upad (~10s do 50% pri 50Hz) — koncert
#define AGC_DECAY_MED      0.995f  // Srednji upad (~3.4s do 50%) — club
#define AGC_DECAY_FAST     0.98f   // Hiter upad (~0.7s do 50%) — testiranje
#define AGC_MIN_FLOOR      1.0f    // Min referenčna vrednost (prepreči deljenje z ~0)

#define DMX_UNIVERSE_SIZE  513   // Start code + 512
#define ARTNET_TIMEOUT_MS  10000  // 10 sekund
#define WS_UPDATE_INTERVAL  80   // ~12 fps posodobitev WebSocket klientov

// ============================================================================
//  DMX KONSTANTE
// ============================================================================
#define DMX_BAUD         250000
#define DMX_BREAK_BAUD    96000

// ============================================================================
//  ENUMI
// ============================================================================

// Način krmiljenja
enum ControlMode : uint8_t {
  CTRL_ARTNET        = 0,  // ArtNet je aktiven, prejema pakete
  CTRL_LOCAL_AUTO    = 1,  // Ni ArtNet-a Xs, samodejni preklop
  CTRL_LOCAL_MANUAL  = 2,  // Uporabnik ročno preklopil
  CTRL_LOCAL_PRIMARY = 3   // Manualna mešalka ima prednost; ArtNet se ignorira dokler operator ne dovoli
};

// Tip kanala (za fixture profile)
enum ChannelType : uint8_t {
  CH_GENERIC    = 0,
  CH_INTENSITY  = 1,
  CH_COLOR_R    = 2,
  CH_COLOR_G    = 3,
  CH_COLOR_B    = 4,
  CH_COLOR_W    = 5,
  CH_COLOR_A    = 6,   // Amber
  CH_COLOR_UV   = 7,
  CH_PAN        = 8,
  CH_PAN_FINE   = 9,
  CH_TILT       = 10,
  CH_TILT_FINE  = 11,
  CH_SPEED      = 12,
  CH_GOBO       = 13,
  CH_SHUTTER    = 14,
  CH_PRESET     = 15,
  CH_PRISM      = 16,
  CH_FOCUS      = 17,
  CH_ZOOM       = 18,
  CH_STROBE     = 19,
  CH_MACRO      = 20,
  CH_COLOR_L    = 21,  // Lime
  CH_COLOR_C    = 22,  // Cyan
  CH_CCT        = 23,  // Barvna temperatura (Color Correlated Temperature)
  CH_COLOR_WW   = 24   // Warm White (ločen od cold white CH_COLOR_W)
};

// ============================================================================
//  STRUKTURE — Fixture Profile
// ============================================================================

struct ChannelRange {
  uint8_t from;
  uint8_t to;
  char label[16];         // Zmanjšano iz 28
};

struct ChannelDef {
  char name[20];
  uint8_t type;           // ChannelType
  uint8_t defaultValue;
  uint8_t rangeCount;
  ChannelRange ranges[MAX_RANGES_PER_CH];
};

struct FixtureProfile {
  char id[32];            // "filename__mode" (e.g., "stairville-hexspot515__11ch")
  char name[32];          // "Name (mode)" prikazano v UI
  uint8_t channelCount;
  ChannelDef channels[MAX_CHANNELS_PER_FX];
  bool loaded;
  uint8_t zoomMin;        // Min zoom kot v stopinjah (0=ni nastavljen, default 10)
  uint8_t zoomMax;        // Max zoom kot v stopinjah (0=ni nastavljen, default 60)
};

// ============================================================================
//  STRUKTURE — Fixture Patch
// ============================================================================

struct PatchEntry {
  char name[20];          // Uporabnikovo ime ("PAR levo 1")
  char profileId[32];     // Referenca na profil (mora se ujemati s FixtureProfile.id)
  uint16_t dmxAddress;    // 1-512
  uint8_t groupMask;      // Bitmask za do 8 skupin
  bool soundReactive;
  bool active;            // Ali je ta slot v uporabi
  int8_t profileIndex;    // Indeks v profileArray[] (-1 = ni naložen)
  // Pan/Tilt omejitve in obračanje osi
  bool invertPan;         // Obrni Pan os (za zrcalno postavljene luči)
  bool invertTilt;        // Obrni Tilt os
  uint8_t panMin;         // Spodnja meja Pan (0-255, privzeto 0)
  uint8_t panMax;         // Zgornja meja Pan (0-255, privzeto 255)
  uint8_t tiltMin;        // Spodnja meja Tilt (0-255, privzeto 0)
  uint8_t tiltMax;        // Zgornja meja Tilt (0-255, privzeto 255)
};

struct GroupDef {
  char name[20];
  bool active;
};

// ============================================================================
//  STRUKTURE — State Snapshot (zgodovina stanj)
// ============================================================================

struct StateSnapshot {
  uint8_t dmx[DMX_MAX_CHANNELS];
  unsigned long timestamp;   // millis() ob snapshot-u
  bool valid;
  char source;               // 'A' = ArtNet stanje, 'L' = Lokalno stanje
};

// ============================================================================
//  STRUKTURE — Scene
// ============================================================================

struct Scene {
  char name[MAX_SCENE_NAME_LEN];
  uint8_t dmx[DMX_MAX_CHANNELS];
  bool valid;
};

// Crossfade stanje
struct CrossfadeState {
  bool     active;
  uint8_t  fromDmx[DMX_MAX_CHANNELS];  // Izhodišče
  uint8_t  toDmx[DMX_MAX_CHANNELS];    // Cilj
  uint32_t durationMs;                   // Trajanje crossfade-a
  uint32_t startTime;                    // millis() ob začetku
  int      targetSceneIdx;               // Katera scena je cilj (-1 = ročno)
};

// Cue List vnos
struct CueEntry {
  int8_t   sceneSlot;                   // Scene slot (0-19), -1 = invalid
  uint16_t fadeMs;                      // Per-cue fade time (0-10000)
  uint16_t autoFollowMs;               // 0 = manual, >0 = auto-advance delay po fade koncu
  char     label[MAX_SCENE_NAME_LEN];  // Override label
};

// ============================================================================
//  STRUKTURE — Sound-to-Light
// ============================================================================

// Krivulja odziva
enum ResponseCurve : uint8_t {
  CURVE_LINEAR = 0,
  CURVE_EXPONENTIAL = 1,
  CURVE_LOGARITHMIC = 2,
  CURVE_SQUARE = 3
};

// Sound preset recepti
enum SoundPreset : uint8_t {
  SPRESET_CUSTOM = 0,     // Ročne nastavitve
  SPRESET_PULSE  = 1,     // Bass → dimmer, oster attack
  SPRESET_RAINBOW = 2,    // Mid → rotacija barv
  SPRESET_STORM  = 3,     // Beat → strobe burst
  SPRESET_AMBIENT = 4,    // Počasno dihanje, mehak
  SPRESET_CLUB   = 5,     // Bass+beat → dimmer, high → strobe
  SPRESET_COUNT  = 6
};

// Fixture zvočna cona
enum SoundZone : uint8_t {
  ZONE_ALL  = 0,   // Vse enako
  ZONE_BASS = 1,   // Reagira na bass
  ZONE_MID  = 2,   // Reagira na mid
  ZONE_HIGH = 3    // Reagira na high
};

// Vir beata
enum BeatSource : uint8_t {
  BSRC_AUDIO   = 0,  // Iz FFT avdio analize
  BSRC_MANUAL  = 1,  // Ročni tap tempo / BPM vpis
  BSRC_AUTO    = 2   // Avdio ko je signal, sicer manual fallback
};

// Programi za manual beat mode
enum ManualBeatProgram : uint8_t {
  MBPROG_PULSE     = 0,   // Intenziteta pada od beata
  MBPROG_CHASE     = 1,   // Zaporedna aktivacija fixture-ov
  MBPROG_SINE      = 2,   // Gladka sinusoidna oscilacija
  MBPROG_STROBE    = 3,   // Ostro vklop/izklop na beat
  MBPROG_RAINBOW   = 4,   // Barvna rotacija sinhro z beatom
  MBPROG_BUILD     = 5,   // Gradnja intenzitete čez N beatov
  MBPROG_RANDOM    = 6,   // Naključne barve ob vsakem beatu
  MBPROG_ALTERNATE = 7,   // Sode/lihe luči izmenjujejo
  MBPROG_WAVE      = 8,   // Sinusni val s faznim zamikom
  MBPROG_STACK     = 9,   // Vsak beat doda luč, potem reset
  MBPROG_SPARKLE   = 10,  // Naključno utripanje
  MBPROG_SCANNER   = 11,  // Ena luč skenira levo-desno
  MBPROG_COUNT     = 12
};

// Beat subdivizija (množilnik)
enum BeatSubdiv : uint8_t {
  SUBDIV_QUARTER = 0,  // 1/4x — počasneje
  SUBDIV_HALF    = 1,  // 1/2x
  SUBDIV_NORMAL  = 2,  // 1x
  SUBDIV_DOUBLE  = 3,  // 2x
  SUBDIV_QUAD    = 4   // 4x — hitreje
};

// FX simetrija (smer izvajanja efektov)
enum FxSymmetry : uint8_t {
  SYM_FORWARD    = 0,  // Normalno (1→2→3→4) — privzeto
  SYM_REVERSE    = 1,  // Obratno (4→3→2→1)
  SYM_CENTER_OUT = 2,  // Iz sredine navzven (3→2→4→1)
  SYM_ENDS_IN    = 3   // Zunaj-noter (1→4→2→3)
};

// Dimmer krivulje za beat programe
enum DimmerCurve : uint8_t {
  DIM_LINEAR      = 0,  // Brez spremembe
  DIM_EXPONENTIAL = 1,  // dimMod^2.5 — bolj dramatično
  DIM_LOGARITHMIC = 2   // sqrt(dimMod) — mehkejše
};

// Barvne palete
enum ColorPalette : uint8_t {
  PAL_RAINBOW = 0,  // Polna HSV rotacija (privzeto)
  PAL_WARM    = 1,  // Rdeča/oranžna/rumena
  PAL_COOL    = 2,  // Modra/vijolična/cyan
  PAL_FIRE    = 3,  // Rdeča/oranžna/bela
  PAL_OCEAN   = 4,  // Cyan/modra/teal
  PAL_PARTY   = 5,  // Magenta/rumena/cyan
  PAL_CUSTOM  = 6,  // Uporabniške 4 barve
  PAL_COUNT   = 7
};

// Program chain (playlist)
#define MAX_CHAIN_ENTRIES 8
struct ProgramChainEntry {
  uint8_t program;        // ManualBeatProgram
  uint8_t durationBeats;  // Koliko beatov pred naslednjim (1-255)
};

struct ProgramChain {
  bool    active;
  uint8_t count;          // 0-MAX_CHAIN_ENTRIES
  ProgramChainEntry entries[MAX_CHAIN_ENTRIES];
};

// Per-group beat override
#define GROUP_BEAT_INHERIT 0xFF

struct GroupBeatOverride {
  uint8_t program;    // ManualBeatProgram ali GROUP_BEAT_INHERIT
  uint8_t subdiv;     // BeatSubdiv ali GROUP_BEAT_INHERIT
  uint8_t intensity;  // 0-100 (%) ali GROUP_BEAT_INHERIT za globalni
};

// Nastavitve manual beat
struct ManualBeatConfig {
  bool     enabled;           // Manual beat aktiven
  uint8_t  source;            // BeatSource
  float    bpm;               // Ročni BPM (30-300)
  uint8_t  program;           // ManualBeatProgram
  uint8_t  subdiv;            // BeatSubdiv
  float    intensity;         // Jakost programa 0.0-1.0
  bool     colorEnabled;      // Program vpliva na barve
  uint8_t  buildBeats;        // Koliko beatov za BUILD program (4-32)
  // --- Faza 3: krivulje + envelope ---
  uint8_t  dimCurve;          // DimmerCurve
  uint16_t attackMs;          // Envelope attack (0=instant, do 500ms)
  uint16_t decayMs;           // Envelope decay (0=programski default, do 2000ms)
  // --- Faza 4: barvne palete ---
  uint8_t  palette;           // ColorPalette
  uint16_t customHues[4];     // Custom paleta (0-360°)
  // --- FX simetrija ---
  uint8_t  symmetry;            // FxSymmetry (smer izvajanja efektov)
  // --- Faza 6: per-group override ---
  GroupBeatOverride groupOverrides[MAX_GROUPS];
};

static const ManualBeatConfig MANUAL_BEAT_DEFAULTS = {
  false,          // enabled
  BSRC_MANUAL,   // source
  120.0f,         // bpm
  MBPROG_PULSE,   // program
  SUBDIV_NORMAL,  // subdiv
  0.8f,           // intensity
  true,           // colorEnabled
  8,              // buildBeats
  DIM_LINEAR,     // dimCurve
  0,              // attackMs
  0,              // decayMs
  PAL_RAINBOW,    // palette
  {0,90,180,270}, // customHues
  SYM_FORWARD,     // symmetry
  {{GROUP_BEAT_INHERIT,GROUP_BEAT_INHERIT,GROUP_BEAT_INHERIT},
   {GROUP_BEAT_INHERIT,GROUP_BEAT_INHERIT,GROUP_BEAT_INHERIT},
   {GROUP_BEAT_INHERIT,GROUP_BEAT_INHERIT,GROUP_BEAT_INHERIT},
   {GROUP_BEAT_INHERIT,GROUP_BEAT_INHERIT,GROUP_BEAT_INHERIT},
   {GROUP_BEAT_INHERIT,GROUP_BEAT_INHERIT,GROUP_BEAT_INHERIT},
   {GROUP_BEAT_INHERIT,GROUP_BEAT_INHERIT,GROUP_BEAT_INHERIT},
   {GROUP_BEAT_INHERIT,GROUP_BEAT_INHERIT,GROUP_BEAT_INHERIT},
   {GROUP_BEAT_INHERIT,GROUP_BEAT_INHERIT,GROUP_BEAT_INHERIT}}
};

// Easy mode nastavitve
struct STLEasyConfig {
  bool     enabled;
  float    sensitivity;     // 0.1 - 5.0 (množilnik)
  float    soundAmount;     // 0.0 - 1.0 (koliko sound vpliva)
  bool     bassIntensity;   // Bass → dimmer
  bool     midColor;        // Mid → barvni premik
  bool     highStrobe;      // High → strobe
  bool     beatBump;        // Beat detect → bump
  uint8_t  preset;          // SoundPreset
  uint8_t  zones[32]; // SoundZone per fixture (fiksno 32 za binarno kompatibilnost)
  float    beatPhase;       // 0.0-1.0 faza v beat ciklu (za beat-sync)
  bool     beatSync;        // Efekti sinhroni z beatom
};

static const STLEasyConfig STL_EASY_DEFAULTS = {
  false,   // enabled
  1.0f,    // sensitivity
  0.5f,    // soundAmount
  true,    // bassIntensity
  true,    // midColor
  true,    // highStrobe
  true,    // beatBump
  SPRESET_CUSTOM,  // preset
  {0},     // zones — vse ZONE_ALL
  0.0f,    // beatPhase
  false    // beatSync
};

// AGC nastavitve (ločena struktura za binarno kompatibilnost z V3)
struct STLAgcConfig {
  float    bandGains[STL_BAND_COUNT]; // Per-band gain množilnik (0.0 - 3.0)
  float    agcSpeed;                   // AGC hitrost prilagajanja (0.0=počasi, 1.0=hitro)
  float    noiseGate;                  // Noise gate prag (0.0=izključen, 1.0=agresiven)
};

static const STLAgcConfig STL_AGC_DEFAULTS = {
  {1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f},  // bandGains — enakomerno
  0.5f,  // agcSpeed — srednji
  0.3f   // noiseGate — zmeren
};

// Pro mode pravilo
struct STLRule {
  bool     active;
  uint8_t  fixtureIdx;      // Kateri fixture
  uint8_t  channelIdx;      // Kateri kanal v fixture-u
  uint16_t freqLow;         // Spodnja frekvenčna meja Hz
  uint16_t freqHigh;        // Zgornja frekvenčna meja Hz
  uint8_t  outMin;          // Minimalna DMX vrednost
  uint8_t  outMax;          // Maksimalna DMX vrednost
  uint8_t  curve;           // ResponseCurve
  uint8_t  attackMs;        // Attack čas / 10 (0-255 → 0-2550ms)
  uint8_t  decayMs;         // Decay čas / 10
};

// FFT rezultat za pošiljanje prek WebSocket
struct FFTBands {
  float bands[STL_BAND_COUNT];   // Normalizirane vrednosti 0.0-1.0
  float bass;                     // Povprečje bass pasu
  float mid;
  float high;
  bool  beatDetected;
  float bpm;
};

// ============================================================================
//  STRUKTURE — Konfiguracija noda (persistentna)
// ============================================================================

// ============================================================================
//  STRUKTURE — Omrežje in konfiguracija
// ============================================================================

#define MAX_WIFI_APS 5

struct WifiAP {
  char ssid[33];
  char password[65];
};

struct NodeConfig {
  char hostname[28];
  uint16_t universe;        // ArtNet univerza (0-32767)
  uint16_t channelCount;    // Koliko DMX kanalov pošiljati (1-512)

  // Omrežje — do 5 AP-jev s failover
  WifiAP wifiAPs[MAX_WIFI_APS];
  bool dhcp;
  char staticIp[16];
  char staticGw[16];
  char staticSn[16];

  // Audio
  uint8_t audioSource;      // 0=off, 1=I2S line-in (WM8782S), 2=I2S mic (INMP441)

  // Avtentikacija
  bool authEnabled;
  char authUser[20];
  char authPass[20];

  // ArtNet vedenje
  uint16_t artnetTimeoutSec;  // Po koliko sekundah brez ArtNet-a preklopi na LOCAL_AUTO (privzeto 10)
  bool artnetPrimaryMode;     // true = CTRL_LOCAL_PRIMARY ob zagonu (manualna mešalka ima prednost)

  // Omrežni protokoli izhod
  bool artnetOutEnabled;      // Oddajaj DMX kot ArtNet broadcast
  bool sacnEnabled;           // Oddajaj DMX kot sACN (E1.31) multicast
};

// Privzete vrednosti
static const NodeConfig DEFAULT_CONFIG = {
  "ARTNET-NODE",   // hostname
  0,               // universe
  512,             // channelCount
  {{"", ""},{"", ""},{"", ""},{"", ""},{"", ""}},  // wifiAPs (prazno = AP mode)
  true,            // dhcp
  "192.168.1.100",
  "192.168.1.1",
  "255.255.255.0",
  2,               // audioSource: I2S mikrofon (INMP441)
  false,           // authEnabled
  "admin",         // authUser
  "",              // authPass (prazno = brez gesla)
  10,              // artnetTimeoutSec
  false,           // artnetPrimaryMode
  false,           // artnetOutEnabled
  false            // sacnEnabled
};

// ============================================================================
//  DATOTEČNE POTI (LittleFS)
// ============================================================================
#define PATH_CONFIG       "/config.json"
#define PATH_PATCH        "/patch.json"
#define PATH_GROUPS       "/groups.json"
#define PATH_SCENES_DIR   "/scenes"
#define PATH_PROFILES_DIR "/profiles"
#define PATH_CONFIGS_DIR  "/configs"
#define PATH_SOUND_CFG    "/sound.bin"

// ============================================================================
//  PSRAM HELPER
// ============================================================================
inline void* psramPreferMalloc(size_t size) {
#if HAS_PSRAM
  if (psramFound()) { void* p = ps_malloc(size); if (p) return p; }
#endif
  return malloc(size);
}

#endif // CONFIG_H
