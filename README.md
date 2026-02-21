# ESP32 / ESP32-S3 ArtNet/DMX Node z mesalno mizo

**Kompletna verzija** — ArtNet node + fixture engine + mixer + scene + sound-to-light + cue list + LFO + OSC + 2D layout + pixel mapper + Ableton Link + ESP-NOW wireless DMX + gamepad + magic wand

## Podprte platforme

| | ESP32 DevKit V1 | ESP32-S3 N16R8 |
|---|---|---|
| **Jedra** | 2x Xtensa LX6 @ 240 MHz | 2x Xtensa LX7 @ 240 MHz |
| **Flash** | 4 MB | 16 MB |
| **PSRAM** | — | 8 MB OPI |
| **SRAM** | 520 KB | 512 KB + PSRAM |
| **USB** | Samo UART (CP2102/CH340) | Nativen USB CDC |
| **Bluetooth** | BT Classic + BLE | Samo BLE |
| **Status LED** | RGB LED (3x PWM) | WS2812 NeoPixel (1 piksel) |
| **FFT resolucija** | 512 vzorcev (~21.5 Hz/bin) | 1024 vzorcev (~10.7 Hz/bin) |
| **FFT knjiznica** | ESP-DSP (hardware-accelerated) | ESP-DSP (Vector ISA / SIMD) |
| **DMX UART** | UART2 | UART1 |
| **Pixel Mapper** | — | WS2812 LED trak (RMT, GPIO 16) |
| **Razpolozljivi GPIO** | 0-39 (z omejitvami) | 0-21, 38-48 (26-37 zasedeni za OPI flash/PSRAM) |

Platforma se zazna samodejno prek `CONFIG_IDF_TARGET_ESP32S3` — ni rocne konfiguracije.

## Funkcionalnosti

- **ArtNet -> DMX** izhod (1 univerza, do 512 kanalov)
- **sACN (E1.31)** multicast izhod (vzporedno z ArtNet)
- **State machine**: ArtNet aktiven -> Lokalno (samodejno po 10s timeout) -> Lokalno (rocno)
- **Fixture profili** (JSON, nalaganje prek spletnega vmesnika)
- **Fixture patch** (ime, DMX naslov, profil, skupine)
- **Spletni mixer** s faderji za vsak kanal izbranega fixture-a
- **Master dimmer** (vpliva na intensity + barvne kanale)
- **HSV->RGBW pretvorba na ESP32** — barvni izbirnik poslje HSV vrednosti na ESP32, ki pretvori HSV->RGB in nato izpelne W/A/UV kanale iz profila fixture-a (1 sporocilo namesto 3-6)
- **Scene** — shrani/recall do 20 scen s crossfade (0-10s)
- **Sound-to-light** — ESP-DSP hardware FFT analiza s parametricnim EQ (nastavljiva center frekvenca + Q za vsak pas), easy mode (bass->dimmer, mid->barve, high->strobe, beat->bump) in pro mode (uporabniska pravila za mapiranje frekvencnih pasov na kanale)
- **ESP-DSP Hardware FFT** — hardware-pospesan FFT z ESP-DSP knjiznico (Vector ISA / SIMD na ESP32-S3), ~3x hitrejse od programske implementacije
- **Audio vhod** — I2S line-in (WM8782S ADC) ali I2S MEMS mikrofon (INMP441)
- **Beat detection** z BPM izracunom
- **Ableton Link** — Wi-Fi BPM/beat sinhronizacija z DJ programi (Ableton Live, Traktor, Rekordbox, Serato, VirtualDJ)
- **Zivi FFT spekter** v spletnem vmesniku
- **Pametni Blackout** — izklopi samo svetlobne kanale (Intensity, barve, strobe). Pan/Tilt/Gobo/Focus/Zoom ostanejo nespremenjeni
- **Flash (Blinder)** — drzi gumb za prisilno 100% intenziteto na vseh fixture-ih (deluje tudi med blackoutom)
- **Celozaslonska konzola** — optimiziran pogled za upravljanje v zivo:
  - Vertikalni sliderji za vse kanale z drag & drop prerazvrscanjem fixture-ov
  - Barvne pike za vizualno stanje fixture-a (long-press = FULL ON na telefonu)
  - Gang checkboxi za skupno premikanje kanalov ali skupinskih dimmerjev
  - Solo kanal (klik na vrednost), fine-tune (dvojni klik za natancno vrednost)
  - Undo/redo, shranjevanje scen z nastavljivo crossfade hitrostjo
  - Skrivanje/razsirjanje posameznih kanalov ali celih fixture-ov
- **Skupinski master dimmerji** — neodvisen intensity slider za vsako skupino + gang, BO, FO, solo na ravni skupine
- **Beat sistem** — 12 programov (Pulse, Chase, Sine, Strobe, Rainbow, Build, Random, Alternate, Wave, Stack, Sparkle, Scanner), subdivizije (1/4x-4x), per-group programi, dimmer krivulje, barvne palete, chaining
- **Manual beat** — tap tempo, rocni BPM, avdio BPM sinhronizacija (samodejno sledenje tempu iz glasbe) ali Ableton Link
- **Pametna detekcija telefona** — samodejno prilagodi UI (skrije beat gumbe, prikaze meni) z JS detekcijo touch naprave
- **Zgodovina stanj** (3 avtomatski snapshot-i ob preklopu)
- **2D Layout Editor** — interaktivni oder s SVG vizualizacijo fixtur, drag & drop pozicioniranje, zoom (20%-300%), pan, barvne pike z realnim DMX izhodom, prosto risanje odrskih elementov, auto-arrange (linija, lok, krog, mreza)
- **Carobna palica (Magic Wand)** — telefon giroskop -> Pan/Tilt kontrola prek DeviceOrientation API
- **Igralni ploscek (Gamepad API)** — podpora za PlayStation/Xbox kontrolerje prek brskalnikovega Gamepad API
- **Filmstrip gumbi** — kanali z obsegi (Gobo, Prism, Macro, Preset, Shutter) prikazejo mrezo gumbov namesto drsnika
- **Fan / Pahljaca** — razprsitev fader vrednosti cez vec izbranih fixture-ov (npr. pahljaca Pan kota)
- **Pan/Tilt omejitve** — per-fixture invert osi in bounding box (min/max) za Pan in Tilt
- **Snap crossfade** — kanali tipa Gobo/Prism/Shutter/Macro/Preset preskocijo na sredini crossfade-a namesto interpolacije
- **Highlight / Locate** — toggle gumb ki nastavi fixture na locate preset (dimmer=255, barve=bela, pan/tilt=center, zoom=wide, gobo=open), ob izklopu obnovi originalne vrednosti
- **Cue List** — sekvencno predvajanje scen s per-cue fade casom in auto-follow zamikom, GO/BACK/STOP kontrola, do 40 cue-jev, persistenca na LittleFS
- **LFO / FX Generator** — do 8 neodvisnih oscilatorjev (sine, triangle, square, sawtooth) za dimmer/pan/tilt/R/G/B, per-fixture fazni spread za chase efekte, FX simetrija (forward, reverse, center-out, ends-in)
- **Shape Generator** — geometricne oblike (krog, osmica, trikotnik, kvadrat, linija) za Pan/Tilt animacije, do 4 neodvisne instance
- **Pixel Mapper (WS2812 LED trak)** — poganja zunanji WS2812/NeoPixel LED trak prek RMT periferne enote (samo ESP32-S3)
- **DMX Output Monitor** — prikaz vseh 512 kanalov v realnem casu na canvas gridu z barvnim kodiranjem vrednosti
- **OSC Server** — UDP port 8000, podpira `/dmx/N`, `/fixture/N/dimmer`, `/fixture/N/color`, `/scene/N`, `/master`, `/blackout`
- **Web MIDI** — podpora za MIDI kontrolerje prek Web MIDI API (Chrome/Edge), MIDI Learn, CC->fader in Note->scene mapping
- **ESP-NOW Wireless DMX** — brezzicni DMX prenos brez ruterja, <1ms latenca, do 4 slave sprejemniki
- **Multiplayer WebSocket Sync** — takojsnja sinhronizacija stanja ob priklopu novega klienta
- **RGB status LED** (zelena=ArtNet, cyan=lokalno/auto, vijolicna=lokalno/rocno)
- **Factory reset** (drzi GPIO14 ob zagonu 3s)

## Strojna oprema

### Primerjava vezav (ESP32 vs ESP32-S3)

```
                          ESP32 DevKit V1          ESP32-S3 N16R8
                          ──────────────           ──────────────
DMX TX (-> MAX485 DI)      GPIO 17 (UART2)          GPIO 17 (UART1)
DMX RX (<- MAX485 RO)     GPIO 16 (UART2)          GPIO 18 (UART1)
DMX DE (-> MAX485 DE+RE)   GPIO 4                   GPIO 8

Status LED rdeca           GPIO 25 (PWM)            -
Status LED zelena          GPIO 26 (PWM)            -
Status LED modra           GPIO 27 (PWM)            -
Status LED NeoPixel        -                        GPIO 48 (WS2812)

Pixel Mapper LED trak      -                        GPIO 16 (WS2812, RMT)

Reset tipka (-> GND)        GPIO 14                  GPIO 14

I2S Line BCK (WM8782S)    GPIO 32                  GPIO 4
I2S Line WS  (WM8782S)    GPIO 33                  GPIO 5
I2S Line DATA (WM8782S)   GPIO 36                  GPIO 6

I2S Mic BCK  (INMP441)    GPIO 22                  GPIO 12
I2S Mic WS   (INMP441)    GPIO 21                  GPIO 11
I2S Mic DATA (INMP441)    GPIO 34                  GPIO 10
```

> **Opomba:** Na ESP32-S3 N16R8 so GPIO 26-37 zasedeni za Octal SPI flash in PSRAM. Zato so vsi pini za avdio in LED premaknjeni na razpolozljive GPIO (0-21, 38-48).

### Vezava — ESP32 DevKit V1

```
ESP32 DevKit V1
|
|-- GPIO17 (TX) --> MAX485 DI
|-- GPIO16 (RX) --> MAX485 RO (neuporabljen v TX nacinu)
|-- GPIO4  (DE) --> MAX485 DE + RE (skupaj)
|
|-- GPIO25 --> RGB LED rdeca (+ 220 Ohm)
|-- GPIO26 --> RGB LED zelena (+ 220 Ohm)
|-- GPIO27 --> RGB LED modra (+ 220 Ohm)
|
|-- GPIO14 --> Reset tipka -> GND (INPUT_PULLUP)
|
|-- GPIO32 --> I2S BCK  (WM8782S line-in ADC)
|-- GPIO33 --> I2S WS   (WM8782S)
|-- GPIO36 --> I2S DATA (WM8782S, input-only GPIO)
|
|-- GPIO22 --> I2S BCK  (INMP441 mikrofon, opcijsko)
|-- GPIO21 --> I2S WS   (INMP441)
'-- GPIO34 --> I2S DATA (INMP441)
```

### Vezava — ESP32-S3 N16R8

```
ESP32-S3 N16R8
|
|-- GPIO17 (TX) --> MAX485 DI
|-- GPIO18 (RX) --> MAX485 RO (neuporabljen v TX nacinu)
|-- GPIO8  (DE) --> MAX485 DE + RE (skupaj)
|
|-- GPIO48 --> Vgrajena WS2812 NeoPixel LED (ni zunanjega vezja)
|
|-- GPIO16 --> Zunanji WS2812 LED trak (Pixel Mapper, do 144 LED-ic)
|
|-- GPIO14 --> Reset tipka -> GND (INPUT_PULLUP)
|
|-- GPIO4  --> I2S BCK  (WM8782S line-in ADC)
|-- GPIO5  --> I2S WS   (WM8782S)
|-- GPIO6  --> I2S DATA (WM8782S)
|
|-- GPIO12 --> I2S BCK  (INMP441 mikrofon, opcijsko)
|-- GPIO11 --> I2S WS   (INMP441)
'-- GPIO10 --> I2S DATA (INMP441)
```

### Pixel Mapper — WS2812 LED trak (samo ESP32-S3)

```
ESP32-S3 GPIO16 ---[330 Ohm]---> WS2812 DIN
ESP32-S3 5V    -----------------> WS2812 VCC
ESP32-S3 GND   -----------------> WS2812 GND

Priporocene prakse:
- 1000 uF kondenzator med VCC in GND na zacetku traku
- 330 Ohm upor na podatkovni liniji (blizu GPIO16)
- Zunanji 5V napajalnik za vec kot 30 LED-ic (3A za 144 LED-ic)
- Najvec 144 LED-ic (omejitev v konfiguraciji)
```

Pixel Mapper uporablja RMT (Remote Control Transceiver) periferno enoto ESP32-S3 za generiranje WS2812 signala brez obremenitve CPU-ja.

### Line-in vezje (enako za obe platformi)

Ce uporabljate WM8782S ADC, ta skrbi za A/D pretvorbo. Ce uporabljate direktni line-in prek ADC (samo ESP32, GPIO36):

```
Audio L --||-- 1uF --+-- GPIO36
                     |
                  10k Ohm -- 3.3V
                     |
                  10k Ohm -- GND
```

### MAX485 -> XLR5 (enako za obe platformi)

```
MAX485            XLR-5 (pogled od spredaj)
  A  ------------ Pin 3 (Data+)
  B  ------------ Pin 2 (Data-)
  GND ----------- Pin 1 (GND)
                  Pin 4 (Data2-) - ni povezan
                  Pin 5 (Data2+) - ni povezan
```

### ESP-NOW Wireless DMX — Slave vezava

```
ESP32 (slave sprejemnik)
|
|-- GPIO17 (TX) --> MAX485 DI
|-- GPIO4  (DE) --> MAX485 DE + RE (skupaj)
|-- GND ---------> MAX485 GND
|
'-- WiFi antena --> ESP-NOW sprejem (<1ms latenca)

Cena slave modula: ESP32 DevKit (~2 EUR) + MAX485 modul (~1 EUR) = ~3 EUR
```

## Arduino IDE nastavitve

### ESP32-S3 N16R8

| Nastavitev | Vrednost |
|---|---|
| **Board** | ESP32S3 Dev Module |
| **USB CDC On Boot** | Enabled |
| **Flash Size** | 16MB (128Mb) |
| **Flash Mode** | QIO 80MHz |
| **PSRAM** | OPI PSRAM |
| **Partition Scheme** | Custom (`partitions.csv` v mapi sketcha) |
| **CPU Frequency** | 240MHz |

### ESP32 DevKit V1

| Nastavitev | Vrednost |
|---|---|
| **Board** | ESP32 Dev Module |
| **Partition Scheme** | No OTA (2MB APP / 2MB SPIFFS) ali Huge APP |
| **Flash Size** | 4MB |
| **Upload Speed** | 921600 |
| **Bluetooth** | Disabled (priporoceno — prihraanek ~70KB DRAM) |

### Knjiznice (Library Manager)

- `ArtnetWifi` by Nathanael Lecaude
- `ESPAsyncWebServer` (mathieucarbou fork za ESP32 core 3.x)
- `AsyncTCP` (GitHub: me-no-dev/AsyncTCP)
- `ArduinoJson` 7.x by Benoit Blanchon
- `ESP-DSP` (vgrajen v ESP32 Arduino core — hardware-accelerated FFT z Vector ISA na ESP32-S3)
- `Adafruit_NeoPixel` (za WS2812 status LED na ESP32-S3 **in** Pixel Mapper LED trak)
- `AbletonLinkESP32` (opcijsko — za Ableton Link beat sync; brez nje deluje kot stub)

### LittleFS upload

Za nalaganje fixture profilov iz `data/` mape:
1. Namesti **ESP32 LittleFS upload plugin**
2. V Arduino IDE: Tools -> ESP32 Sketch Data Upload
3. Ali pa profile nalozi prek spletnega vmesnika (Fixtures -> Upload)

## Uporaba

### Prvo zaganjeno

1. ESP32 ustvari AP: **ARTNET-NODE** (geslo: `artnetdmx`)
2. Povezi se in odpri `http://192.168.4.1`
3. V zavihku **Nastavitve** nastavi WiFi SSID in geslo
4. Shrani -> ESP32 se ponovno zazene in poveze na omrezje

### Fixture profili

Profili so JSON datoteke v formatu:

```json
{
  "name": "LED PAR RGBW 7ch",
  "channels": [
    {"name": "Dimmer", "type": "intensity", "default": 0},
    {"name": "Red",    "type": "color_r",   "default": 0},
    {"name": "Strobe", "type": "strobe",    "default": 0, "ranges": [
      {"from": 0, "to": 9, "label": "Off"},
      {"from": 10, "to": 255, "label": "Slow -> Fast"}
    ]}
  ]
}
```

**Podprti tipi kanalov:**
`intensity`, `color_r`, `color_g`, `color_b`, `color_w`, `color_a`, `color_uv`,
`pan`, `pan_fine`, `tilt`, `tilt_fine`, `speed`, `gobo`, `shutter`, `preset`,
`prism`, `focus`, `zoom`, `strobe`, `macro`, `generic`,
`color_l` (lime), `color_c` (cyan), `cct`, `color_ww` (warm white)

### State machine

```
ARTNET AKTIVEN
  | (10s brez paketov)
  |-----------------> LOKALNO (auto)
  |                     | (ArtNet se vrne)
  |                     '----> ARTNET AKTIVEN
  | (uporabnik klikne "Lokalno")
  '-----------------> LOKALNO (rocno)
                        | (ignorira ArtPoll)
                        | (uporabnik klikne "ArtNet")
                        '----> ARTNET AKTIVEN
```

Ob vsakem preklopu iz lokalne kontrole se stanje avtomatsko shrani v zgodovino.

### LED barve

| Barva | Pomen |
|-------|-------|
| Zelena (utripa) | ArtNet aktiven, prejema pakete |
| Zelena (stalna) | ArtNet aktiven, ni paketov |
| Cyan | Lokalna kontrola (samodejni preklop) |
| Vijolicna | Lokalna kontrola (rocni preklop) |
| Modra (utripa) | Povezovanje na WiFi |
| Rumena | AP mode |
| Rdeca | Caka na factory reset |
| Rdeca (utripa) | Safe mode (po WDT resetih) |

> Na ESP32 se barve prikazejo prek 3x PWM (RGB LED), na ESP32-S3 pa prek WS2812 NeoPixel na GPIO48. Obnasanje je enako.

## Datotecna struktura

```
esp32_artnet_dmx/
|-- esp32_artnet_dmx.ino   — Glavna datoteka (setup/loop)
|-- config.h               — Konstante, enumi, strukture, GPIO pini za obe platformi
|-- config_store.h         — LittleFS load/save
|-- dmx_driver.h/.cpp      — DMX TX (baud-rate break)
|-- fixture_engine.h/.cpp  — Profili, patch, skupine
|-- mixer_engine.h/.cpp    — State machine, kanali, snapshoti, locate, scene + sound + LFO
|-- scene_engine.h/.cpp    — Scene CRUD, crossfade interpolacija, cue list
|-- audio_input.h/.cpp     — Audio vhod (I2S WM8782S / I2S INMP441), jedro 0
|-- sound_engine.h/.cpp    — ESP-DSP FFT, pasovi, beat detect, easy/pro mode, Ableton Link
|-- lfo_engine.h/.cpp      — LFO/FX generator (8 oscilatorjev, 4 valovne oblike, simetrija)
|-- shape_engine.h/.cpp    — Shape generator (krogi, osmicke, trikotniki za Pan/Tilt)
|-- pixel_mapper.h/.cpp    — Pixel Mapper WS2812 LED trak (samo ESP32-S3, RMT)
|-- link_beat.h/.cpp       — Ableton Link beat sync (stub ce knjiznica ni nalozena)
|-- espnow_dmx.h/.cpp      — ESP-NOW wireless DMX prenos (do 4 slave sprejemnikov)
|-- sacn_output.h/.cpp     — sACN (E1.31) multicast izhod
|-- osc_server.h/.cpp      — OSC UDP server (port 8000)
|-- led_status.h           — RGB LED (PWM za ESP32, NeoPixel za ESP32-S3)
|-- web_ui.h/.cpp          — Web server, API, servira gzipan HTML
|-- web_ui_gz.h            — Gzipan index.html (generiran iz convert.py)
|-- convert.py             — Generira web_ui_gz.h iz index.html (gzip + PROGMEM)
|-- partitions.csv         — Custom particijska tabela za ESP32-S3 (16MB flash)
|-- index.html             — Spletni vmesnik (7 zavihkov + celozaslonska konzola + 2D layout)
'-- data/
    '-- profiles/          — Fixture profili (JSON)
```

## Scene

### Shranjevanje
Scene se shranjujejo v LittleFS v binarnem formatu (24B ime + 512B DMX = 536B na sceno).
Do 20 scen, skupaj max ~11KB na flash-u.

### Crossfade
Linearna interpolacija med trenutnim in ciljnim stanjem:
```
output[ch] = from[ch] + (to[ch] - from[ch]) x (elapsed / duration)
```
Fiksno-vejicni izracun (alpha 0-256) za hitrost na ESP32. Crossfade se posodablja vsak loop() cikel (~40fps).

## Cue List

Sekvencno predvajanje scen z gumbi GO / BACK / STOP. Do **40 cue-jev**, vsak s:
- **Scene slot** — katera od 20 scen se predvaja
- **Fade cas** — per-cue crossfade (0-10s)
- **Auto-follow** — samodejni prehod na naslednji cue po nastavljenem zamiku (0 = rocni trigger)
- **Label** — oznaka cue-ja (do 24 znakov)

Cue list se shrani v `/cuelist.json` na LittleFS. Upravljanje prek spletnega vmesnika (Scene zavihek) ali WebSocket ukazov.

## Sound-to-Light

### Arhitektura
- **Jedro 0**: Audio vzorcenje (I2S) -> polni buffer
- **Jedro 1**: ESP-DSP FFT obdelava (512 ali 1024 vzorcev, Hamming okno), band ekstrakcija, beat detection
- Rezultat: 8 frekvencnih pasov + bass/mid/high + BPM + beat flag
- Modulacija: `dmxOut = manualValue + soundModifier x soundAmount`

> ESP32-S3 s PSRAM uporablja 1024-tockovni FFT (~10.7 Hz/bin resolucija), ESP32 brez PSRAM pa 512-tockovni (~21.5 Hz/bin) za manjso porabo DRAM.

### ESP-DSP Hardware FFT

Sistem uporablja ESP-DSP knjiznico namesto programskega ArduinoFFT. Kljucne prednosti:

| Lastnost | Opis |
|---|---|
| **Hardware pospesenost** | Vector ISA (SIMD) instrukcije na ESP32-S3 Xtensa LX7 |
| **Hamming okno** | Vnaprej izracunano in shranjeno v pomnilniku (`_window[]`) |
| **Buffer format** | Interleaved kompleksni buffer `[re, im, re, im, ...]` za ESP-DSP kompatibilnost |
| **PSRAM alokacija** | FFT bufferji alocirani v PSRAM na ESP32-S3 (prek `psramPreferMalloc`) |
| **Hitrost** | ~3x hitrejse od ArduinoFFT programske implementacije |

Inicializacija ESP-DSP:
```cpp
// Alokacija interleaved kompleksnega bufferja v PSRAM
_fftBuf = (float*)psramPreferMalloc(FFT_SAMPLES * 2 * sizeof(float));
// Inicializacija ESP-DSP FFT tabel
dsps_fft2r_init_fc32(NULL, FFT_SAMPLES);
// Vnaprej izracunano Hamming okno
for (int i = 0; i < FFT_SAMPLES; i++)
  _window[i] = 0.54f - 0.46f * cosf(2.0f * M_PI * i / (FFT_SAMPLES - 1));
```

### Audio viri

| Vir | Vmesnik | Vzorcna frekvenca | Opis |
|---|---|---|---|
| **WM8782S ADC** | I2S slave | 96 kHz (decim. 4x -> 24 kHz) | Line-in, 24-bit, profesionalna kvaliteta |
| **INMP441** | I2S master | 22050 Hz | MEMS mikrofon, kompakten, za ambientno zaznavo |

### Easy mode
| Efekt | Frekvencni pas | Odziv |
|-------|---------------|-------|
| Bass -> Dimmer | 60-250 Hz | Intenziteta utripa z bassom |
| Mid -> Barve | 250-2000 Hz | R/G/B kanali se premikajo |
| High -> Strobe | 2000-8000 Hz | Strobe kanal sledi viskom |
| Beat -> Bump | Detekcija | Dimmer skok ob beatu |

### Pro mode
Uporabniska pravila: izberi fixture, kanal, frekvencni pas, DMX razpon, krivuljo odziva (linearna/eksponentna/logaritmicna), attack/decay cas.

### Beat detection
Primerja trenutno bass energijo s tekocim povprecjem zadnjih 24 vzorcev. Beat = presezek 1.5x povprecja z minimalnim intervalom 200ms. BPM iz povprecja intervalov.

### Beat izvori (Beat Source)

| Vir | Enum | Opis |
|---|---|---|
| **Audio** | `BSRC_AUDIO` | Iz FFT avdio analize |
| **Manual** | `BSRC_MANUAL` | Rocni tap tempo / BPM vpis |
| **Auto** | `BSRC_AUTO` | Avdio ko je signal, sicer manual fallback |
| **Ableton Link** | `BSRC_LINK` | Wi-Fi sinhronizacija z DJ software |

## Ableton Link

Integracija z Ableton Link protokolom za BPM in beat grid sinhronizacijo prek Wi-Fi UDP multicast.

### Kompatibilna programska oprema
- **Ableton Live** — DAW
- **Traktor** — DJ software (Native Instruments)
- **Rekordbox** — DJ software (Pioneer DJ)
- **Serato DJ** — DJ software
- **VirtualDJ** — DJ software

### Delovanje

1. V zavihku **Nastavitve** izberite beat vir: **Ableton Link**
2. ESP32 se prijavi na Link omrezje prek Wi-Fi UDP multicast
3. Spletni vmesnik prikazuje:
   - **Status povezave** — ali je Link aktiven
   - **Stevilo vrstnikov (peers)** — koliko naprav je na Link omrezju
   - **Sinhroniziran BPM** — tempo iz Link omrezja
4. Beat programi samodejno sledijo Link beatu

### Pogojno prevajanje

```cpp
#if __has_include(<ableton/Link.h>)
  #define HAS_ABLETON_LINK 1
#else
  #define HAS_ABLETON_LINK 0  // Stub — ne dela nicesar
#endif
```

Ce Ableton Link ESP32 knjiznica **ni nalozena**, se modul prevede kot stub (brez napak) — vse funkcije vrnejo privzete vrednosti. Za polno delovanje je potrebna knjiznica:

- GitHub: https://github.com/pschatzmann/esp32-esp-idf-abl-link
- Namestitev: kopiraj `include/ableton` mapo v `Arduino/libraries/AbletonLink/`

## ESP-NOW Wireless DMX

Brezzicni prenos DMX podatkov na oddaljene slave sprejemnike prek ESP-NOW protokola — peer-to-peer, brez Wi-Fi ruterja, z latenco pod 1ms.

### Arhitektura

```
ESP32 (master)                    ESP32 (slave 1) + MAX485
   |                                   |
   |-- ESP-NOW broadcast -->           |-- DMX buffer sestavljanje
   |    (3 paketi x 200B)             |-- UART TX -> MAX485 -> XLR
   |                                   |
   |                              ESP32 (slave 2) + MAX485
   |                                   |
   |-- ESP-NOW broadcast -->           |-- DMX buffer sestavljanje
        (3 paketi x 200B)             '-- UART TX -> MAX485 -> XLR
```

### Protokol

DMX buffer (512 bajtov) se fragmentira v 3 pakete po 200 bajtov (ESP-NOW omejitev = 250B):

```
Paket: [SEQ:1][OFFSET_HI:1][OFFSET_LO:1][LEN:1][DATA:do 200]

SEQ       — Zaporedna stevilka (0-255, wraparound) za detekcijo izgubljenih paketov
OFFSET    — Odmik v DMX bufferju (0, 200, 400)
LEN       — Dolzina podatkov v tem paketu (do 200)
DATA      — DMX podatki
```

### Konfiguracija

- V zavihku **Nastavitve** omogocite ESP-NOW
- Dodajte slave sprejemnike po MAC naslovu (do 4 peerov)
- Konfiguracija se shrani v `/espnow.bin` na LittleFS

### Slave sprejemnik

Slave naprava je preprost ESP32 z MAX485 modulom (~3 EUR skupaj):
1. Prejme ESP-NOW pakete od masterja
2. Sestavi celoten DMX buffer iz fragmentov (na podlagi OFFSET polja)
3. Poslje sestavljen buffer prek UART na MAX485

Do **4 slave sprejemnikov** istocasno.

## HSV->RGBW pretvorba na ESP32

Barvni izbirnik v spletnem vmesniku poslje HSV (Hue, Saturation, Value) vrednosti na ESP32 namesto posameznih R/G/B drsnikov. ESP32 nato:

1. **Pretvori HSV -> RGB** — standardna pretvorba
2. **Izpelne W/A/UV kanale** — pregleda fixture profil za prisotnost kanalov tipa `color_w`, `color_a`, `color_uv`
3. **Nastavi vse barvne kanale hkrati** — en WebSocket ukaz namesto 3-6

### Prednosti
- **Manj WebSocket prometa** — 1 sporocilo namesto 3-6 (po eno za vsak barvni kanal)
- **Profilno ozavesceno** — samodejno prepozna W/A/UV kanale iz fixture profila
- **Bolj intuitivno** — uporabnik izbira barvo na HSV kolesu, ESP32 poskrbi za preslikavo

## Pixel Mapper (WS2812 LED trak)

> **Samo ESP32-S3** — zahteva RMT periferno enoto in GPIO 16.

Poganja zunanji WS2812/NeoPixel LED trak prek RMT periferne enote (zero CPU overhead). Uporabno za ambientno osvetlitev odra, LED trakove na truss-ih ali vizualni feedback za DJ-e.

### Nacini delovanja

| Nacin | Opis |
|---|---|
| **Fixture Mirror** | Preslikaj barve iz DMX fixture-ov na LED trak — vsak fixture dobi svoj segment |
| **VU Meter** | Audio nivo indikator: zelena (bass) -> rumena (mid) -> rdeca (high) |
| **Spectrum** | 8 FFT frekvencnih pasov kot mavricne stolpice |
| **Beat Pulse** | Vsi LED-i utripajo na beat z rotirajo cim barvnega odtenka |

### Konfiguracija

V zavihku **Nastavitve**:
- **Stevilo LED-ic** — do 144
- **Svetilnost** — 0-255
- **Nacin** — Fixture Mirror / VU Meter / Spectrum / Beat Pulse
- **Fixture/skupina** — za nacin Fixture Mirror: kateri fixture ali skupina se preslikava

Konfiguracija se shrani v `/pixmap.bin` na LittleFS.

### Primer: Fixture Mirror z 60 LED-ici in 4 fixture-i

```
LED trak:  [===fixture 1===][===fixture 2===][===fixture 3===][===fixture 4===]
            LED 0-14         LED 15-29        LED 30-44        LED 45-59
            barva: rdeca     barva: modra     barva: zelena    barva: bela
```

## Multiplayer WebSocket Sync

Ko se nov WebSocket klient poveze (npr. drugi telefon ali racunalnik), master ESP32 **takoj poslje celotno stanje**:
- Vrednosti vseh DMX kanalov
- Scene, fixture patch, beat nastavitve
- Trenutni nacin delovanja

### Prednosti
- **Brez cakanja** — ni potrebe po periodicnem broadcast ciklu
- **Vec naprav** — vec uporabnikov na razlicnih napravah vidi in upravlja enako stanje v realnem casu
- **Konzistentnost** — vsak nov klient takoj dobi aktualno stanje

## Carobna palica (Magic Wand)

Telefonski giroskop se uporabi za kontrolo Pan/Tilt kanalov izbranih fixture-ov prek brskalnikovega DeviceOrientation API.

### Uporaba

1. Odprite **2D Layout Editor** na telefonu
2. Kliknite na fixture -> odpre se popup s sliderji
3. Kliknite na **zeleno XY piko** -> odpre se XY Pad popup
4. Pritisnite gumb **Carobna palica** za vklop/izklop
5. Drzite telefon in nagibajte za kontrolo Pan (levo/desno) in Tilt (naprej/nazaj)

### Delovanje

- **Kalibracija** — ob prvem nagibu se nastavi nicla tocka (current orientation = center)
- **Razpon** — privzeto 50 stopinj (nastavljivo)
- **Osi** — nagib levo/desno = Pan, naprej/nazaj = Tilt
- **Zahteve** — deluje samo na mobilnih napravah z giroskopom (iPhone, Android telefoni)
- **API** — brskalnikov DeviceOrientation API (zahteva HTTPS ali localhost)

## Igralni ploscek (Gamepad API)

Podpora za PlayStation in Xbox kontrolerje prek brskalnikovega Gamepad API. Omogoca fizicno kontrolo svetlobne konzole brez dotikanja zaslona.

### Mapiranje kontrolerja

| Kontroler element | Funkcija |
|---|---|
| **Levi stick (X/Y)** | Skupina 1 — XY Pad (Pan/Tilt) |
| **Desni stick (X/Y)** | Skupina 2 — XY Pad (Pan/Tilt) |
| **R2 trigger** | Master dimmer (0-100%) |
| **X gumb** | Flash (drzi za prisilno 100% intenziteto) |
| **B gumb** | Blackout toggle (vklop/izklop) |

### Nastavitve

- **Mrtva cona (deadzone)** — privzeto 15%, nastavljivo (preprecuje drift pri stickah v mirovanju)
- **Vklop/izklop** — v zavihku **Nastavitve**

### Podprti brskalniki
- Google Chrome (desktop + Android)
- Microsoft Edge (desktop)
- Deluje s standardnimi PlayStation (DualShock/DualSense) in Xbox kontrolerji

## LFO / FX Generator

Do **8 neodvisnih oscilatorjev** za modulacijo DMX kanalov. Integriran v mixer pipeline za sound overlay in pred master dimmer.

### Valovne oblike

| Oblika | Formula |
|--------|---------|
| Sine | `sin(phase x 2pi)` |
| Triangle | Linearni ramp gor/dol |
| Square | On/off pulz |
| Sawtooth | Zagast val |

### Tarcni kanali
Dimmer, Pan, Tilt, Red, Green, Blue

### Parametri (per LFO)
- **Rate**: 0.1-10 Hz
- **Depth**: 0-100% modulacijska globina
- **Phase spread**: 0-100% fazni zamik med fixturami (za chase efekte)
- **Symmetry**: Forward, Reverse, Center-Out, Ends-In — vpliva na razporeditev faznega zamika
- **Fixture mask**: bitmask za izbiro do 24 fixtur

## Shape Generator

Do **4 neodvisne instance** geometricnih oblik za Pan/Tilt animacije.

### Oblike

| Oblika | Opis |
|--------|------|
| Circle | Krozno gibanje |
| Figure 8 | Osmicka |
| Triangle | Trikotno gibanje |
| Square | Kvadratno gibanje |
| Line H | Horizontalna linija |
| Line V | Vertikalna linija |

### Parametri
- **Rate**: 0.1-5.0 Hz
- **Size X/Y**: 0-100% globina Pan/Tilt
- **Phase spread**: 0-100% fazni zamik med fixturami
- **Fixture mask**: 24-bit bitmask

## OSC Server

Vgrajen UDP Open Sound Control (OSC) streznik na **portu 8000** za daljinsko krmiljenje iz zunanjih aplikacij (TouchOSC, QLab, ipd.).

| OSC naslov | Argumenti | Akcija |
|---|---|---|
| `/dmx/N` | float (0-1) | Nastavi DMX kanal N |
| `/fixture/N/dimmer` | float (0-1) | Nastavi dimmer fixture-a N |
| `/fixture/N/color` | float, float, float | Nastavi R, G, B fixture-a N |
| `/scene/N` | — | Priclici sceno N (s crossfade 1.5s) |
| `/master` | float (0-1) | Master dimmer |
| `/blackout` | int (0/1) | Blackout on/off |

## Celozaslonska konzola

Optimiziran pogled za zivo upravljanje luci (telefon, tablica ali desktop).

### Fixture upravljanje
- Vertikalni sliderji za vse kanale vsakega fixture-a
- **Drag & drop** prerazvrscanje fixture-ov
- **Solo kanal** — klik na vrednost skrije vse ostale kanale fixture-a
- **Fine-tune** — dvojni klik na vrednost za vnos natancnega stevila
- **FULL ON** — drzi gumb (ali long-press na barvno piko na telefonu)
- **Gang** — oznaci checkboxe na vec kanalih/skupinah, premik enega premakne vse

### Skupine
- Skupinski master dimmerji z gang podporo
- Per-group **Blackout**, **Full On**, **Solo**
- Per-group izbira beat programa in subdivizije (desktop)

### Beat programi (12)

| Program | Opis |
|---------|------|
| Pulse | Dimmer pulziranje |
| Chase | Zaporedna aktivacija fixture-ov |
| Sine | Gladko sinusno valovanje |
| Strobe | Hitro utripanje |
| Rainbow | Barvni cikel |
| Build | Postopna gradnja intenzitete |
| Random | Nakljucni vzorci |
| Alternate | Izmenicno preklapljanje |
| Wave | Sireci se val |
| Stack | Postopno dodajanje fixture-ov |
| Sparkle | Nakljucno iskricenje |
| Scanner | Efekt premikajocega zarka |

- **Subdivizije**: 1/4x, 1/2x, 1x, 2x, 4x
- **Dimmer krivulje**: linearna, eksponentna, logaritmicna
- **Barvne palete**: Rainbow, Warm, Cool, Fire, Ocean, Party, Custom
- **Chaining**: zaporedno predvajanje vec programov
- **Manual beat**: tap tempo ali rocni BPM brez zvocnega vira

### Telefon
- Beat programi se prikazejo v popup meniju (namesto inline gumbov)
- Long-press na barvno piko sprozi FULL ON
- Detekcija telefona z JS (`hover: none` + `pointer: coarse` + velikost zaslona)

## 2D Layout Editor

Interaktivni vizualni oder za pozicioniranje luci v prostoru.

- **SVG vizualizacija** z realnim DMX izhodom — barvne pike prikazujejo dejansko barvo in intenziteto
- **Drag & drop** pozicioniranje fixtur iz stranske vrstice na oder
- **Zoom** 20%-300% (scroll kolo) s **pan** (vleci prazno povrsino)
- **Odrski elementi** — prosto risanje oblik (pravokotniki, elipse) za vizualni kontekst
- **Fixture popup** — klik na fixture odpre panel s sliderji za vse kanale, Locate in barvnim izbirnikom
- **Zelena pika (XY)** — odpre XY Pad popup za natancno Pan/Tilt kontrolo z relativnim vlecenjem
- **Modra pika (Zoom)** — drag ali scroll kolo za zoom kontrolo
- **Carobna palica** — gumb v XY Pad popupu za vklop telefon-giroskop Pan/Tilt kontrole
- **Shranjevanje layoutov** na LittleFS z moznostjo vec poimenovanih layoutov

## Posodabljanje spletnega vmesnika

Po urejanju `index.html` je potrebno regenerirati `web_ui_gz.h`:

```bash
python3 convert.py
```

Skripta prebere `index.html`, ga gzip kompresira (level 9) in zapise C PROGMEM array v `web_ui_gz.h`.

## Watchdog in Safe Mode

Sistem ima 8-sekundni watchdog timer. Po 3 zaporednih WDT resetih se aktivira **safe mode**, ki onemogoci FFT/sound procesiranje (pogost vzrok za crash). Safe mode je signaliziran z rdecim utripanjem LED.

## RAM poraba (ocena)

### ESP32 (320 KB SRAM)

| Komponenta | KB |
|---|---|
| WiFi stack | ~50 |
| AsyncWebServer + WS | ~15 |
| DMX bufferji (3x512) | ~1.5 |
| Fixture profili | ~12 |
| Snapshoti (3x512) | ~1.5 |
| Scene (crossfade 2x512) | ~1 |
| Cue list (40x30B) | ~1.2 |
| FFT buffer (2x512x4B) | ~4 |
| FFT Hamming okno (512x4B) | ~2 |
| Sound engine | ~2 |
| LFO engine (8 instanc) | ~0.25 |
| Shape generator (4 instance) | ~0.1 |
| OSC server (256B buffer) | ~0.3 |
| Locate states (24 fixtur) | ~0.6 |
| ESP-NOW bufferji + config | ~0.5 |
| Ableton Link (stub) | ~0.05 |
| Audio task (jedro 0) | ~4 |
| FreeRTOS | ~24 |
| **Skupaj** | **~120** |
| **Prosto (od 320KB)** | **~200** |

### ESP32-S3 (512 KB SRAM + 8 MB PSRAM)

Na ESP32-S3 se FFT bufferji in vecji podatkovni bloki alocirajo v PSRAM (prek `psramPreferMalloc`), kar sprosti interno SRAM za WiFi stack in FreeRTOS. FFT uporablja 1024 vzorcev (2x resolucija). Flash prostor za LittleFS je ~9.9 MB (vs ~2 MB na ESP32).

| Komponenta | SRAM | PSRAM |
|---|---|---|
| WiFi stack | ~50 KB | — |
| AsyncWebServer + WS | ~15 KB | — |
| DMX bufferji (3x512) | ~1.5 KB | — |
| Fixture profili | — | ~12 KB |
| FFT buffer (2x1024x4B) | — | ~8 KB |
| FFT Hamming okno (1024x4B) | — | ~4 KB |
| Sound engine | ~2 KB | — |
| Pixel Mapper (Adafruit_NeoPixel) | ~0.5 KB | ~0.5 KB (LED buffer) |
| ESP-NOW bufferji + config | ~0.5 KB | — |
| Ableton Link (ce aktiven) | ~2 KB | — |
| FreeRTOS | ~24 KB | — |
| **Skupaj SRAM** | **~96 KB** | |
| **Skupaj PSRAM** | | **~25 KB** |
| **Prosto SRAM (od 512KB)** | **~416 KB** | |
| **Prosto PSRAM (od 8MB)** | | **~8167 KB** |

## LittleFS datoteke

| Pot | Opis | Velikost |
|---|---|---|
| `/config.json` | Omrezna konfiguracija, audio vir, ArtNet nastavitve | ~0.5 KB |
| `/patch.json` | Fixture patch (imena, naslovi, profili, skupine) | ~2 KB |
| `/groups.json` | Definicije skupin | ~0.3 KB |
| `/scenes/` | Scene (binarne datoteke, do 20) | ~11 KB |
| `/profiles/` | Fixture profili (JSON) | odvisno od stevila |
| `/cuelist.json` | Cue list | ~2 KB |
| `/sound.bin` | Sound-to-light konfiguracija | ~0.5 KB |
| `/pixmap.bin` | Pixel Mapper konfiguracija | ~0.02 KB |
| `/espnow.bin` | ESP-NOW peer konfiguracija | ~0.1 KB |
| `/configs/` | Shranjene konfiguracije (do 8) | ~4 KB |

## Licenca

MIT
