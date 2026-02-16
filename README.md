# ESP32 ArtNet/DMX Node z mešalno mizo

**Kompletna verzija** — ArtNet node + fixture engine + mixer + scene + sound-to-light

## Funkcionalnosti

- **ArtNet → DMX** izhod (1 univerza, do 512 kanalov)
- **State machine**: ArtNet aktiven → Lokalno (samodejno po 10s timeout) → Lokalno (ročno)
- **Fixture profili** (JSON, nalaganje prek spletnega vmesnika)
- **Fixture patch** (ime, DMX naslov, profil, skupine)
- **Spletni mixer** s faderji za vsak kanal izbranega fixture-a
- **Master dimmer** (vpliva na intensity + barvne kanale)
- **Scene** — shrani/recall do 20 scen s crossfade (0-10s)
- **Sound-to-light** — FFT analiza, easy mode (bass→dimmer, mid→barve, high→strobe, beat→bump) in pro mode (uporabniška pravila za mapiranje frekvenčnih pasov na kanale)
- **Audio vhod** — ADC line-in (GPIO36) ali I2S MEMS mikrofon (INMP441)
- **Beat detection** z BPM izračunom
- **Živi FFT spekter** v spletnem vmesniku
- **Blackout** gumb
- **Zgodovina stanj** (5 avtomatskih snapshot-ov ob preklopu)
- **RGB status LED** (zelena=ArtNet, cyan=lokalno/auto, vijolična=lokalno/ročno)
- **Factory reset** (drži GPIO14 ob zagonu 3s)

## Strojna oprema

### Vezava

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
├── GPIO36 ──→ Audio line-in (ADC1_CH0)
│              (DC bias: 2×10kΩ delilnik + 1µF AC coupling)
│
├── GPIO22 ──→ I2S BCK  (INMP441 mikrofon, opcijsko)
├── GPIO21 ──→ I2S WS   (INMP441)
└── GPIO34 ──→ I2S DATA (INMP441)
```

### Line-in vezje
```
Audio L ──┤├── 1µF ──┬── GPIO36
                     │
                  10kΩ ── 3.3V
                     │
                  10kΩ ── GND
```

### MAX485 → XLR5

```
MAX485            XLR-5 (pogled od spredaj)
  A  ──────────── Pin 3 (Data+)
  B  ──────────── Pin 2 (Data-)
  GND ─────────── Pin 1 (GND)
                  Pin 4 (Data2-) ─ ni povezan
                  Pin 5 (Data2+) ─ ni povezan
```

## Arduino IDE nastavitve

1. **Board**: ESP32 Dev Module
2. **Partition Scheme**: Default 4MB with spiffs (ali "No OTA (2MB APP / 2MB SPIFFS)")
3. **Flash Size**: 4MB
4. **Upload Speed**: 921600

### Knjižnice (Library Manager)

- `ArtnetWifi` by Nathanaël Lécaudé
- `ESPAsyncWebServer` (GitHub: me-no-dev/ESPAsyncWebServer)
- `AsyncTCP` (GitHub: me-no-dev/AsyncTCP)
- `ArduinoJson` 7.x by Benoît Blanchon
- `arduinoFFT` by Enrique Condes (za sound-to-light)

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
`prism`, `focus`, `zoom`, `strobe`, `macro`, `generic`

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

## Datotečna struktura

```
esp32_artnet_dmx/
├── esp32_artnet_dmx.ino   — Glavna datoteka (setup/loop)
├── config.h               — Konstante, enumi, strukture
├── config_store.h         — LittleFS load/save
├── dmx_driver.h/.cpp      — DMX TX (baud-rate break)
├── fixture_engine.h/.cpp  — Profili, patch, skupine
├── mixer_engine.h/.cpp    — State machine, kanali, snapshoti, scene + sound
├── scene_engine.h/.cpp    — Scene CRUD, crossfade interpolacija
├── audio_input.h/.cpp     — Audio vhod (ADC / I2S), jedro 0
├── sound_engine.h/.cpp    — FFT, pasovi, beat detect, easy/pro mode
├── led_status.h           — RGB LED
├── web_ui.h/.cpp          — Web server, API, vgrajen HTML (5 zavihkov)
└── data/
    ├── profiles/          — Fixture profili (JSON)
    │   ├── par-rgbw-7ch.json
    │   ├── par-rgblac-12ch.json
    │   └── mh-16ch.json
    └── scenes/            — Scene (binarni, 536B/scena)
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

## Sound-to-Light

### Arhitektura
- **Jedro 0**: Audio vzorčenje (ADC pri 22050Hz ali I2S) → polni buffer
- **Jedro 1**: FFT obdelava (1024 vzorcev, Hamming okno), band ekstrakcija, beat detection
- Rezultat: 8 frekvenčnih pasov + bass/mid/high + BPM + beat flag
- Modulacija: `dmxOut = manualValue + soundModifier × soundAmount`

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
Primerja trenutno bass energijo s tekočim povprečjem zadnjih 32 vzorcev. Beat = presežek 1.5× povprečja z minimalnim intervalom 200ms. BPM iz povprečja intervalov.

## RAM poraba (ocena)

| Komponenta | KB |
|---|---|
| WiFi stack | ~50 |
| AsyncWebServer + WS | ~15 |
| DMX bufferji (3×512) | ~1.5 |
| Fixture profili | ~12 |
| Snapshoti (5×512) | ~2.5 |
| Scene (crossfade 2×512) | ~1 |
| FFT buffer (2×1024×4B) | ~8 |
| Sound engine | ~2 |
| Audio task (jedro 0) | ~4 |
| FreeRTOS | ~24 |
| **Skupaj** | **~120** |
| **Prosto (od 320KB)** | **~200** |
