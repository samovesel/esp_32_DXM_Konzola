# ESP32 / ESP32-S3 ArtNet/DMX Node z mešalno mizo

**Kompletna verzija** — ArtNet node + fixture engine + mixer + scene + sound-to-light + cue list + LFO + OSC + 2D layout

## Podprte platforme

| | ESP32 DevKit V1 | ESP32-S3 N16R8 |
|---|---|---|
| **Jedra** | 2× Xtensa LX6 @ 240 MHz | 2× Xtensa LX7 @ 240 MHz |
| **Flash** | 4 MB | 16 MB |
| **PSRAM** | — | 8 MB OPI |
| **SRAM** | 520 KB | 512 KB + PSRAM |
| **USB** | Samo UART (CP2102/CH340) | Nativen USB CDC |
| **Bluetooth** | BT Classic + BLE | Samo BLE |
| **Status LED** | RGB LED (3× PWM) | WS2812 NeoPixel (1 piksel) |
| **FFT resolucija** | 512 vzorcev (~21.5 Hz/bin) | 1024 vzorcev (~10.7 Hz/bin) |
| **DMX UART** | UART2 | UART1 |
| **Razpoložljivi GPIO** | 0–39 (z omejitvami) | 0–21, 38–48 (26–37 zasedeni za OPI flash/PSRAM) |

Platforma se zazna samodejno prek `CONFIG_IDF_TARGET_ESP32S3` — ni ročne konfiguracije.

## Funkcionalnosti

- **ArtNet → DMX** izhod (1 univerza, do 512 kanalov)
- **State machine**: ArtNet aktiven → Lokalno (samodejno po 10s timeout) → Lokalno (ročno)
- **Fixture profili** (JSON, nalaganje prek spletnega vmesnika)
- **Fixture patch** (ime, DMX naslov, profil, skupine)
- **Spletni mixer** s faderji za vsak kanal izbranega fixture-a
- **Master dimmer** (vpliva na intensity + barvne kanale)
- **Scene** — shrani/recall do 20 scen s crossfade (0-10s)
- **Sound-to-light** — FFT analiza, easy mode (bass→dimmer, mid→barve, high→strobe, beat→bump) in pro mode (uporabniška pravila za mapiranje frekvenčnih pasov na kanale)
- **Audio vhod** — I2S line-in (WM8782S ADC) ali I2S MEMS mikrofon (INMP441)
- **Beat detection** z BPM izračunom
- **Živi FFT spekter** v spletnem vmesniku
- **Pametni Blackout** — izklopi samo svetlobne kanale (Intensity, barve, strobe). Pan/Tilt/Gobo/Focus/Zoom ostanejo nespremenjeni
- **Flash (Blinder)** — drži gumb za prisilno 100% intenziteto na vseh fixture-ih (deluje tudi med blackoutom)
- **Celozaslonska konzola** — optimiziran pogled za upravljanje v živo:
  - Vertikalni sliderji za vse kanale z drag & drop prerazvrščanjem fixture-ov
  - Barvne pike za vizualno stanje fixture-a (long-press = FULL ON na telefonu)
  - Gang checkboxi za skupno premikanje kanalov ali skupinskih dimmerjev
  - Solo kanal (klik na vrednost), fine-tune (dvojni klik za natančno vrednost)
  - Undo/redo, shranjevanje scen z nastavljivo crossfade hitrostjo
  - Skrivanje/razširjanje posameznih kanalov ali celih fixture-ov
- **Skupinski master dimmerji** — neodvisen intensity slider za vsako skupino + gang, BO, FO, solo na ravni skupine
- **Beat sistem** — 12 programov (Pulse, Chase, Sine, Strobe, Rainbow, Build, Random, Alternate, Wave, Stack, Sparkle, Scanner), subdivizije (1/4x–4x), per-group programi, dimmer krivulje, barvne palete, chaining
- **Manual beat** — tap tempo ali ročni BPM brez zvočnega vira
- **Pametna detekcija telefona** — samodejno prilagodi UI (skrije beat gumbe, prikaže ☰ meni) z JS detekcijo touch naprave
- **Zgodovina stanj** (3 avtomatski snapshot-i ob preklopu)
- **2D Layout Editor** — interaktivni oder s SVG vizualizacijo fixtur, drag & drop pozicioniranje, zoom (20%–300%), pan, barvne pike z realnim DMX izhodom, prosto risanje odrskih elementov, auto-arrange (linija, lok, krog, mreža)
- **Filmstrip gumbi** — kanali z obsegi (Gobo, Prism, Macro, Preset, Shutter) prikažejo mrežo gumbov namesto drsnika
- **Fan / Pahljača** — razpršitev fader vrednosti čez več izbranih fixture-ov (npr. pahljača Pan kota)
- **Pan/Tilt omejitve** — per-fixture invert osi in bounding box (min/max) za Pan in Tilt
- **Snap crossfade** — kanali tipa Gobo/Prism/Shutter/Macro/Preset preskočijo na sredini crossfade-a namesto interpolacije
- **Highlight / Locate** — toggle gumb ki nastavi fixture na locate preset (dimmer=255, barve=bela, pan/tilt=center, zoom=wide, gobo=open), ob izklopu obnovi originalne vrednosti
- **Cue List** — sekvenčno predvajanje scen s per-cue fade časom in auto-follow zamikom, GO/BACK/STOP kontrola, do 40 cue-jev, persistenca na LittleFS
- **LFO / FX Generator** — do 8 neodvisnih oscilatorjev (sine, triangle, square, sawtooth) za dimmer/pan/tilt/R/G/B, per-fixture fazni spread za chase efekte, FX simetrija (forward, reverse, center-out, ends-in)
- **DMX Output Monitor** — prikaz vseh 512 kanalov v realnem času na canvas gridu z barvnim kodiranjem vrednosti
- **OSC Server** — UDP port 8000, podpira `/dmx/N`, `/fixture/N/dimmer`, `/fixture/N/color`, `/scene/N`, `/master`, `/blackout`
- **Web MIDI** — podpora za MIDI kontrolerje prek Web MIDI API (Chrome/Edge), MIDI Learn, CC→fader in Note→scene mapping
- **RGB status LED** (zelena=ArtNet, cyan=lokalno/auto, vijolična=lokalno/ročno)
- **Factory reset** (drži GPIO14 ob zagonu 3s)

## Strojna oprema

### Primerjava vezav (ESP32 vs ESP32-S3)

```
                          ESP32 DevKit V1          ESP32-S3 N16R8
                          ──────────────           ──────────────
DMX TX (→ MAX485 DI)      GPIO 17 (UART2)          GPIO 17 (UART1)
DMX RX (← MAX485 RO)     GPIO 16 (UART2)          GPIO 18 (UART1)
DMX DE (→ MAX485 DE+RE)   GPIO 4                   GPIO 8

Status LED rdeča           GPIO 25 (PWM)            ─
Status LED zelena          GPIO 26 (PWM)            ─
Status LED modra           GPIO 27 (PWM)            ─
Status LED NeoPixel        ─                        GPIO 48 (WS2812)

Reset tipka (→ GND)        GPIO 14                  GPIO 14

I2S Line BCK (WM8782S)    GPIO 32                  GPIO 4
I2S Line WS  (WM8782S)    GPIO 33                  GPIO 5
I2S Line DATA (WM8782S)   GPIO 36                  GPIO 6

I2S Mic BCK  (INMP441)    GPIO 22                  GPIO 12
I2S Mic WS   (INMP441)    GPIO 21                  GPIO 11
I2S Mic DATA (INMP441)    GPIO 34                  GPIO 10
```

> **Opomba:** Na ESP32-S3 N16R8 so GPIO 26–37 zasedeni za Octal SPI flash in PSRAM. Zato so vsi pini za avdio in LED premaknjeni na razpoložljive GPIO (0–21, 38–48).

### Vezava — ESP32 DevKit V1

```
ESP32 DevKit V1
│
├── GPIO17 (TX) ──→ MAX485 DI
├── GPIO16 (RX) ──→ MAX485 RO (neuporabljen v TX načinu)
├── GPIO4  (DE) ──→ MAX485 DE + RE (skupaj)
│
├── GPIO25 ──→ RGB LED rdeča (+ 220Ω)
├── GPIO26 ──→ RGB LED zelena (+ 220Ω)
├── GPIO27 ──→ RGB LED modra (+ 220Ω)
│
├── GPIO14 ──→ Reset tipka → GND (INPUT_PULLUP)
│
├── GPIO32 ──→ I2S BCK  (WM8782S line-in ADC)
├── GPIO33 ──→ I2S WS   (WM8782S)
├── GPIO36 ──→ I2S DATA (WM8782S, input-only GPIO)
│
├── GPIO22 ──→ I2S BCK  (INMP441 mikrofon, opcijsko)
├── GPIO21 ──→ I2S WS   (INMP441)
└── GPIO34 ──→ I2S DATA (INMP441)
```

### Vezava — ESP32-S3 N16R8

```
ESP32-S3 N16R8
│
├── GPIO17 (TX) ──→ MAX485 DI
├── GPIO18 (RX) ──→ MAX485 RO (neuporabljen v TX načinu)
├── GPIO8  (DE) ──→ MAX485 DE + RE (skupaj)
│
├── GPIO48 ──→ Vgrajena WS2812 NeoPixel LED (ni zunanjega vezja)
│
├── GPIO14 ──→ Reset tipka → GND (INPUT_PULLUP)
│
├── GPIO4  ──→ I2S BCK  (WM8782S line-in ADC)
├── GPIO5  ──→ I2S WS   (WM8782S)
├── GPIO6  ──→ I2S DATA (WM8782S)
│
├── GPIO12 ──→ I2S BCK  (INMP441 mikrofon, opcijsko)
├── GPIO11 ──→ I2S WS   (INMP441)
└── GPIO10 ──→ I2S DATA (INMP441)
```

### Line-in vezje (enako za obe platformi)

Če uporabljate WM8782S ADC, ta skrbi za A/D pretvorbo. Če uporabljate direktni line-in prek ADC (samo ESP32, GPIO36):

```
Audio L ──┤├── 1µF ──┬── GPIO36
                     │
                  10kΩ ── 3.3V
                     │
                  10kΩ ── GND
```

### MAX485 → XLR5 (enako za obe platformi)

```
MAX485            XLR-5 (pogled od spredaj)
  A  ──────────── Pin 3 (Data+)
  B  ──────────── Pin 2 (Data-)
  GND ─────────── Pin 1 (GND)
                  Pin 4 (Data2-) ─ ni povezan
                  Pin 5 (Data2+) ─ ni povezan
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
| **Bluetooth** | Disabled (priporočeno — prihrani ~70KB DRAM) |

### Knjižnice (Library Manager)

- `ArtnetWifi` by Nathanaël Lécaudé
- `ESPAsyncWebServer` (mathieucarbou fork za ESP32 core 3.x)
- `AsyncTCP` (GitHub: me-no-dev/AsyncTCP)
- `ArduinoJson` 7.x by Benoît Blanchon
- `arduinoFFT` by Enrique Condes (za sound-to-light)
- `Adafruit_NeoPixel` (samo ESP32-S3 — za WS2812 status LED)

### LittleFS upload

Za nalaganje fixture profilov iz `data/` mape:
1. Namesti **ESP32 LittleFS upload plugin**
2. V Arduino IDE: Tools → ESP32 Sketch Data Upload
3. Ali pa profile naloži prek spletnega vmesnika (Fixtures → Upload)

## Uporaba

### Prvo zagonjeno

1. ESP32 ustvari AP: **ARTNET-NODE** (geslo: `artnetdmx`)
2. Poveži se in odpri `http://192.168.4.1`
3. V zavihku **Nastavitve** nastavi WiFi SSID in geslo
4. Shrani → ESP32 se ponovni zažene in poveže na omrežje

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
      {"from": 10, "to": 255, "label": "Slow → Fast"}
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
  │ (10s brez paketov)
  ├─────────────────→ LOKALNO (auto)
  │                     │ (ArtNet se vrne)
  │                     └────→ ARTNET AKTIVEN
  │ (uporabnik klikne "Lokalno")
  └─────────────────→ LOKALNO (ročno)
                        │ (ignorira ArtPoll)
                        │ (uporabnik klikne "ArtNet")
                        └────→ ARTNET AKTIVEN
```

Ob vsakem preklopu iz lokalne kontrole se stanje avtomatsko shrani v zgodovino.

### LED barve

| Barva | Pomen |
|-------|-------|
| Zelena (utripa) | ArtNet aktiven, prejema pakete |
| Zelena (stalna) | ArtNet aktiven, ni paketov |
| Cyan | Lokalna kontrola (samodejni preklop) |
| Vijolična | Lokalna kontrola (ročni preklop) |
| Modra (utripa) | Povezovanje na WiFi |
| Rumena | AP mode |
| Rdeča | Čaka na factory reset |
| Rdeča (utripa) | Safe mode (po WDT resetih) |

> Na ESP32 se barve prikažejo prek 3× PWM (RGB LED), na ESP32-S3 pa prek WS2812 NeoPixel na GPIO48. Obnašanje je enako.

## Datotečna struktura

```
esp32_artnet_dmx/
├── esp32_artnet_dmx.ino   — Glavna datoteka (setup/loop)
├── config.h               — Konstante, enumi, strukture, GPIO pini za obe platformi
├── config_store.h         — LittleFS load/save
├── dmx_driver.h/.cpp      — DMX TX (baud-rate break)
├── fixture_engine.h/.cpp  — Profili, patch, skupine
├── mixer_engine.h/.cpp    — State machine, kanali, snapshoti, locate, scene + sound + LFO
├── scene_engine.h/.cpp    — Scene CRUD, crossfade interpolacija, cue list
├── audio_input.h/.cpp     — Audio vhod (I2S WM8782S / I2S INMP441), jedro 0
├── sound_engine.h/.cpp    — FFT, pasovi, beat detect, easy/pro mode
├── lfo_engine.h/.cpp      — LFO/FX generator (8 oscilatorjev, 4 valovne oblike, simetrija)
├── shape_generator.h/.cpp — Shape generator (krogi, osmičke, trikotniki za Pan/Tilt)
├── osc_server.h/.cpp      — OSC UDP server (port 8000)
├── led_status.h           — RGB LED (PWM za ESP32, NeoPixel za ESP32-S3)
├── web_ui.h/.cpp          — Web server, API, servira gzipan HTML
├── web_ui_gz.h            — Gzipan index.html (generiran iz convert.py)
├── convert.py             — Generira web_ui_gz.h iz index.html (gzip + PROGMEM)
├── partitions.csv         — Custom particijska tabela za ESP32-S3 (16MB flash)
├── index.html             — Spletni vmesnik (7 zavihkov + celozaslonska konzola + 2D layout)
└── data/
    └── profiles/          — Fixture profili (JSON)
```

## Scene

### Shranjevanje
Scene se shranjujejo v LittleFS v binarnem formatu (24B ime + 512B DMX = 536B na sceno).
Do 20 scen, skupaj max ~11KB na flash-u.

### Crossfade
Linearna interpolacija med trenutnim in ciljnim stanjem:
```
output[ch] = from[ch] + (to[ch] - from[ch]) × (elapsed / duration)
```
Fiksno-vejični izračun (alpha 0-256) za hitrost na ESP32. Crossfade se posodablja vsak loop() cikel (~40fps).

## Cue List

Sekvenčno predvajanje scen z gumbi GO / BACK / STOP. Do **40 cue-jev**, vsak s:
- **Scene slot** — katera od 20 scen se predvaja
- **Fade čas** — per-cue crossfade (0–10s)
- **Auto-follow** — samodejni prehod na naslednji cue po nastavljenem zamiku (0 = ročni trigger)
- **Label** — oznaka cue-ja (do 24 znakov)

Cue list se shrani v `/cuelist.json` na LittleFS. Upravljanje prek spletnega vmesnika (Scene zavihek) ali WebSocket ukazov.

## Sound-to-Light

### Arhitektura
- **Jedro 0**: Audio vzorčenje (I2S) → polni buffer
- **Jedro 1**: FFT obdelava (512 ali 1024 vzorcev, Hamming okno), band ekstrakcija, beat detection
- Rezultat: 8 frekvenčnih pasov + bass/mid/high + BPM + beat flag
- Modulacija: `dmxOut = manualValue + soundModifier × soundAmount`

> ESP32-S3 s PSRAM uporablja 1024-točkovni FFT (~10.7 Hz/bin resolucija), ESP32 brez PSRAM pa 512-točkovni (~21.5 Hz/bin) za manjšo porabo DRAM.

### Audio viri

| Vir | Vmesnik | Vzorčna frekvenca | Opis |
|---|---|---|---|
| **WM8782S ADC** | I2S slave | 96 kHz (decim. 4× → 24 kHz) | Line-in, 24-bit, profesionalna kvaliteta |
| **INMP441** | I2S master | 22050 Hz | MEMS mikrofon, kompakten, za ambientno zaznavo |

### Easy mode
| Efekt | Frekvenčni pas | Odziv |
|-------|---------------|-------|
| Bass → Dimmer | 60-250 Hz | Intenziteta utripa z bassom |
| Mid → Barve | 250-2000 Hz | R/G/B kanali se premikajo |
| High → Strobe | 2000-8000 Hz | Strobe kanal sledi viškom |
| Beat → Bump | Detekcija | Dimmer skok ob beatu |

### Pro mode
Uporabniška pravila: izberi fixture, kanal, frekvenčni pas, DMX razpon, krivuljo odziva (linearna/eksponentna/logaritmična), attack/decay čas.

### Beat detection
Primerja trenutno bass energijo s tekočim povprečjem zadnjih 24 vzorcev. Beat = presežek 1.5× povprečja z minimalnim intervalom 200ms. BPM iz povprečja intervalov.

## LFO / FX Generator

Do **8 neodvisnih oscilatorjev** za modulacijo DMX kanalov. Integriran v mixer pipeline za sound overlay in pred master dimmer.

### Valovne oblike

| Oblika | Formula |
|--------|---------|
| Sine | `sin(phase × 2π)` |
| Triangle | Linearni ramp gor/dol |
| Square | On/off pulz |
| Sawtooth | Žagast val |

### Tarčni kanali
Dimmer, Pan, Tilt, Red, Green, Blue

### Parametri (per LFO)
- **Rate**: 0.1–10 Hz
- **Depth**: 0–100% modulacijska globina
- **Phase spread**: 0–100% fazni zamik med fixturami (za chase efekte)
- **Symmetry**: Forward, Reverse, Center-Out, Ends-In — vpliva na razporeditev faznega zamika
- **Fixture mask**: bitmask za izbiro do 24 fixtur

## OSC Server

Vgrajen UDP Open Sound Control (OSC) strežnik na **portu 8000** za daljinsko krmiljenje iz zunanjih aplikacij (TouchOSC, QLab, ipd.).

| OSC naslov | Argumenti | Akcija |
|---|---|---|
| `/dmx/N` | float (0–1) | Nastavi DMX kanal N |
| `/fixture/N/dimmer` | float (0–1) | Nastavi dimmer fixture-a N |
| `/fixture/N/color` | float, float, float | Nastavi R, G, B fixture-a N |
| `/scene/N` | — | Prikliči sceno N (s crossfade 1.5s) |
| `/master` | float (0–1) | Master dimmer |
| `/blackout` | int (0/1) | Blackout on/off |

## Celozaslonska konzola

Optimiziran pogled za živo upravljanje luči (telefon, tablica ali desktop).

### Fixture upravljanje
- Vertikalni sliderji za vse kanale vsakega fixture-a
- **Drag & drop** prerazvrščanje fixture-ov
- **Solo kanal** — klik na vrednost skrije vse ostale kanale fixture-a
- **Fine-tune** — dvojni klik na vrednost za vnos natačnega števila
- **FULL ON** — drži gumb (ali long-press na barvno piko na telefonu)
- **Gang** — označi checkboxe na več kanalih/skupinah, premik enega premakne vse

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
| Random | Naključni vzorci |
| Alternate | Izmenično preklapljanje |
| Wave | Šireči se val |
| Stack | Postopno dodajanje fixture-ov |
| Sparkle | Naključno iskričenje |
| Scanner | Efekt premikajočega žarka |

- **Subdivizije**: 1/4x, 1/2x, 1x, 2x, 4x
- **Dimmer krivulje**: linearna, eksponentna, logaritmična
- **Barvne palete**: Rainbow, Warm, Cool, Fire, Ocean, Party, Custom
- **Chaining**: zaporedno predvajanje več programov
- **Manual beat**: tap tempo ali ročni BPM brez zvočnega vira

### Telefon
- Beat programi se prikažejo v ☰ popup meniju (namesto inline gumbov)
- Long-press na barvno piko sproži FULL ON
- Detekcija telefona z JS (`hover: none` + `pointer: coarse` + velikost zaslona)

## 2D Layout Editor

Interaktivni vizualni oder za pozicioniranje luči v prostoru.

- **SVG vizualizacija** z realnim DMX izhodom — barvne pike prikazujejo dejansko barvo in intenziteto
- **Drag & drop** pozicioniranje fixtur iz stranske vrstice na oder
- **Zoom** 20%–300% (scroll kolo) s **pan** (vleci prazno površino)
- **Odrski elementi** — prosto risanje oblik (pravokotniki, elipse) za vizualni kontekst
- **Fixture popup** — klik na fixture odpre panel s sliderji za vse kanale, Locate in barvnim izbirnikom
- **Zelena pika (XY)** — odpre XY Pad popup za natančno Pan/Tilt kontrolo z relativnim vlečenjem
- **Modra pika (Zoom)** — drag ali scroll kolo za zoom kontrolo
- **Shranjevanje layoutov** na LittleFS z možnostjo več poimenovanih layoutov

## Posodabljanje spletnega vmesnika

Po urejanju `index.html` je potrebno regenerirati `web_ui_gz.h`:

```bash
python3 convert.py
```

Skripta prebere `index.html`, ga gzip kompresira (level 9) in zapiše C PROGMEM array v `web_ui_gz.h`.

## Watchdog in Safe Mode

Sistem ima 8-sekundni watchdog timer. Po 3 zaporednih WDT resetih se aktivira **safe mode**, ki onemogoči FFT/sound procesiranje (pogost vzrok za crash). Safe mode je signaliziran z rdečim utripanjem LED.

## RAM poraba (ocena)

### ESP32 (320 KB SRAM)

| Komponenta | KB |
|---|---|
| WiFi stack | ~50 |
| AsyncWebServer + WS | ~15 |
| DMX bufferji (3×512) | ~1.5 |
| Fixture profili | ~12 |
| Snapshoti (3×512) | ~1.5 |
| Scene (crossfade 2×512) | ~1 |
| Cue list (40×30B) | ~1.2 |
| FFT buffer (2×512×4B) | ~4 |
| Sound engine | ~2 |
| LFO engine (8 instanc) | ~0.25 |
| OSC server (256B buffer) | ~0.3 |
| Locate states (24 fixtur) | ~0.6 |
| Audio task (jedro 0) | ~4 |
| FreeRTOS | ~24 |
| **Skupaj** | **~117** |
| **Prosto (od 320KB)** | **~203** |

### ESP32-S3 (512 KB SRAM + 8 MB PSRAM)

Na ESP32-S3 se FFT bufferji in večji podatkovni bloki alocirajo v PSRAM (prek `psramPreferMalloc`), kar sprosti interno SRAM za WiFi stack in FreeRTOS. FFT uporablja 1024 vzorcev (2× resolucija). Flash prostor za LittleFS je ~9.9 MB (vs ~2 MB na ESP32).
