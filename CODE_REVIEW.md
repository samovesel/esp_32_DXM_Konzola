# Pregled kode: ESP32 ArtNet/DMX Konzola

## Kaj je ta projekt?

To je **profesionalni sistem za upravljanje razsvetljave** na osnovi ESP32, ki zdruzuje:

- **ArtNet / sACN --> DMX512 pretvornik** (gateway) prek WiFi
- **Samostojno mesalno mizo** (mixer) z web vmesnikom
- **Zvocno reaktivno razsvetljavo** z ESP-DSP hardware FFT analizo, parametricnim EQ (nastavljiva center frekvenca in Q faktor za vsak pas) in detekcijo udarca (beat detection)
- **Upravljanje profilov svetil** in scen s crossfade funkcijo
- **Ableton Link** sinhronizacijo s DJ programsko opremo (BPM/beat grid)
- **ESP-NOW brezvicni DMX** za peer-to-peer oddajanje na slave sprejemnike (<1ms latenca)
- **Pixel Mapper** za krmiljenje WS2812 NeoPixel LED trakov (ESP32-S3)
- **LFO in Shape generatorje** za gibalne vzorce moving head svetil
- **OSC** nadzor prek zunanjih kontrolerjev

**Verzija:** 1.0.0
**Jezik:** C++ (Arduino/ESP-IDF za ESP32)
**Platforme:** ESP32 DevKit V1 (dual-core, 4MB Flash, 320KB DRAM) **in** ESP32-S3 N16R8 (dual-core, 16MB Flash, 8MB PSRAM)

---

## Arhitektura (dvojedrna)

| Jedro | Naloge |
|-------|--------|
| **Core 1** (Arduino loop) | ArtNet branje UDP, OSC server, mixer posodabljanje, LFO/Shape update, DMX izhod (40 fps), sACN/ArtNet OUT oddajanje, ESP-NOW oddajanje, Pixel Mapper update, WebSocket |
| **Core 0** (auxTask) | Audio vzorcenje (I2S), ESP-DSP FFT obdelava, beat detekcija, Ableton Link update, LED status, WiFi/TCP |

Nitna varnost je zagotovljena s FreeRTOS muteksom (`SemaphoreHandle_t`) za DMX medpomnilnike. WebSocket handler zaklene mixer pred vsako operacijo, ker AsyncTCP tece na Core 0, mixer update pa na Core 1.

---

## Struktura projekta

```
esp_32_DXM_Konzola/
+-- esp32_artnet_dmx.ino           Glavni sketch (setup/loop, dvojedrni zagon)
+-- config.h                        Konstante, strukture, enumeracije, GPIO, platformska logika
+-- config_store.h                  JSON persistenca (NodeConfig, Patch, Groups)
+-- dmx_driver.h/cpp               UART DMX prenos (break + start code)
+-- fixture_engine.h/cpp           Profili svetil + upravljanje patcha
+-- mixer_engine.h/cpp             Stavni avtomat, mesanje kanalov, scene, flash/locate
+-- scene_engine.h/cpp             Scene: shranjevanje/nalaganje, crossfade, cue list
+-- audio_input.h/cpp              Audio vzorcenje (I2S WM8782S / INMP441), dvojno medpomnjenje
+-- sound_engine.h/cpp             ESP-DSP FFT, AGC, beat detekcija, zvocna pravila, Link integracija
+-- lfo_engine.h/cpp               LFO generator (sine/triangle/square/sawtooth) za dimmer/pan/tilt/barve
+-- shape_engine.h/cpp             Shape generator (krog, osmica, trikotnik...) za moving head
+-- pixel_mapper.h/cpp             WS2812 LED trak Pixel Mapper (samo ESP32-S3)
+-- link_beat.h/cpp                Ableton Link beat sinhronizacija (pogojna kompilacija)
+-- espnow_dmx.h/cpp              ESP-NOW brezvicni DMX oddajnik (master)
+-- osc_server.h/cpp               OSC protokol server (UDP port 8000)
+-- sacn_output.h/cpp              sACN (E1.31) multicast izhod
+-- led_status.h                    RGB LED indikacija stanja (PWM)
+-- web_ui.h/cpp                   Web streznik, REST API, WebSocket, HSV konverzija
+-- web_ui_gz.h                    Stisnjen index.html (gzip, ~93KB)
+-- index.html                      Web UI (7 zavihkov, ~5500 vrstic)
+-- partitions.csv                  Prilagojena particijska tabela za ESP32-S3 (16MB)
+-- data/
|   +-- profiles/                  Definicije svetil (29 JSON datotek)
|   +-- scenes/                    Binarne scene (536B vsaka)
+-- build_personas.sh              Gzip kompresija persona datotek za LittleFS
+-- personas/
|   +-- persona-core.js            Skupna JS knjiznica (WebSocket, PWA, config)
|   +-- portal.html                Portal za izbiro persone
|   +-- staff.html                 Osebje lokala (veliki gumbi)
|   +-- sound-eng.html             Tonski mojster (scene, STL, FFT)
|   +-- theater.html               Gledalisce (GO/BACK, cue list)
|   +-- dj.html                    DJ (BPM, tap, mood preseti)
|   +-- event.html                 Poroke/zabave (timeline, auto-advance)
|   +-- busker.html                Ulicni performer (STL, 4 looki)
|   +-- manifest-*.json            PWA manifesti (7x)
+-- README.md                       Dokumentacija (slovenscina)
+-- PROFILE_SPEC.md               Specifikacija profilov svetil v2.0
+-- CODE_REVIEW.md                 Ta dokument
+-- CONFIG.md                       Dokumentacija konfiguracije
+-- EFFECTS.md                     Dokumentacija efektov
+-- MIXER.md                       Dokumentacija mixerja
+-- SCENES.md                      Dokumentacija scen
+-- SOUND.md                       Dokumentacija zvocnega modula
```

**Skupno vrstic kode:** ~11.500 (C++/Arduino) + ~5.500 (HTML/JS/CSS v index.html) + ~1.500 (persona HTML/JS)

---

## Kljucni moduli

### 1. DMX Driver (`dmx_driver.h/cpp`)
- UART prenos pri 250kHz, format 8N2
- Break signal: preklop na 96kHz za ~94us LOW pulz
- Protokol: Start Code (0x00) + 512 DMX kanalov
- ESP32: UART2, ESP32-S3: UART1 (avtomatska izbira prek `config.h`)

### 2. Fixture Engine (`fixture_engine.h/cpp`)
- Nalaganje profilov svetil iz LittleFS `/profiles/` (JSON format)
- Podpora za vecnacinovne profile (npr. 4ch, 7ch, 16ch v eni datoteki)
- Do 24 svetil v patchu, 8 skupin (bitmask)
- 25 tipov kanalov: intensity, color (r/g/b/w/ww/a/uv/l/c), pan/tilt (+ fine), strobe, gobo, prism, focus, zoom, CCT, speed, macro, preset
- Pan/Tilt omejitve in obracanje osi (invertPan, invertTilt, panMin/Max, tiltMin/Max)

### 3. Mixer Engine (`mixer_engine.h/cpp`)
- Stavni avtomat: ARTNET <-> LOCAL_AUTO <-> LOCAL_MANUAL <-> LOCAL_PRIMARY
- Dva DMX medpomnilnika: `artnetShadow` (vedno posodobljen) in `manualValues` (lokalne spremembe)
- Master dimmer, group dimmerji (8 skupin), snapshoti (3 zadnja stanja za undo)
- 1-sekundno mehko prehajanje med nacini (mode crossfade)
- Flash (blinder) in Locate funkcija za svetila
- Master Speed mnozilnik za LFO/Shape hitrost
- Integracija z LFO Engine, Shape Generator in Sound Engine prek `applyToOutput()`

### 4. Scene Engine (`scene_engine.h/cpp`)
- Do 20 scen v LittleFS binarnem formatu (24B ime + 512B DMX = 536B)
- Crossfade z linearno interpolacijo (0-10 sekund)
- Fiksna aritmetika (alpha 0-256) za ucinkovitost ESP32
- Cue List: do 40 cue-jev z auto-advance in per-cue fade time

### 5. Audio Input (`audio_input.h/cpp`)
- Dvojno medpomnjenje na Core 0 v loceni FreeRTOS nalogi
- **I2S line-in (WM8782S):** 24-bit ADC, 96kHz master mode z decimacijo 4x na ~24kHz
- **I2S mikrofon (INMP441):** 24-bit digitalni MEMS mikrofon
- Podpora za ESP32-S3 N16R8 z locenima GPIO nabora za line-in in mikrofon

### 6. Sound Engine (`sound_engine.h/cpp`) -- POSODOBLJEN
- **ESP-DSP hardware FFT:** zamenjava ArduinoFFT z ESP-DSP knjiznico
  - Hardware pospesevalnik prek Vector ISA na ESP32-S3
  - In-place radix-2 FFT z `dsps_fft2r_fc32()` + bit reversal
  - Interleaved complex buffer format `[re, im, re, im, ...]` alociran v PSRAM
  - **Pre-computed Hamming okno:** izracunano ob inicializaciji, aplicirano ob vsaki FFT iteraciji
- **FFT velikost:** 512 vzorcev (ESP32 DRAM) ali 1024 vzorcev (ESP32-S3 s PSRAM)
  - Resolucija: ~21.5 Hz/bin (512) ali ~10.7 Hz/bin (1024) pri 22050 Hz
- 8 frekvencnih pasov: sub-bas (30 Hz) do diskant (11 kHz)
- **AGC (Automatic Gain Control):**
  - Per-band peak tracking z nastavljivim decay (AGC_DECAY_SLOW/MED/FAST)
  - Minimalna referenca `AGC_MIN_FLOOR = 1.0f` za preprecitev deljenja z 0
  - Per-band gain mnozilniki (0.0 - 3.0)
  - Nastavljiva AGC hitrost in noise gate prag
- **Beat detekcija** (neodvisna od parametricnega EQ — bere surove FFT bine):
  - Nastavljiv prag obcutljivosti (0.8x–2.5x drseče povprečje, privzeto 1.4x)
  - Nastavljiv frekvenčni pas (privzeto 30–150 Hz, 4 preseti: Sub, Bass, Wide, Kick)
  - Nastavljiv lockout interval (100–500ms, privzeto 200ms)
  - Median-filtered BPM iz 16 inter-beat intervalov (robustnejši od povprečenja)
- **Ableton Link integracija:**
  - `LinkBeat _link` objekt integriran v SoundEngine
  - Ko je `BSRC_LINK` izbran, se BPM in beat faza berejo iz Link protokola
  - Link update tece vedno (za peer discovery), neodvisno od avdio signala
- **Easy nacin:** 5 prednastavitev (Pulse, Rainbow, Storm, Ambient, Club)
  - Per-fixture zone (Bass/Mid/High/All)
  - Beat sync z fazo
- **Manual beat nacin:** 12 programov (Pulse, Chase, Sine, Strobe, Rainbow, Build, Random, Alternate, Wave, Stack, Sparkle, Scanner)
  - Subdivizije (1/4x do 4x), dimmer krivulje, intensity envelope (attack/decay)
  - 7 barvnih palet (Rainbow, Warm, Cool, Fire, Ocean, Party, Custom)
  - FX simetrija (Forward, Reverse, Center-Out, Ends-In)
  - Program chain (playlist do 8 programov z beat trajanjem)
  - Per-group beat override (program, subdiv, intensity per skupina)
  - **5 virov beata:** Manual (tap/drsnik), Avdio BPM (FFT sync), Auto fallback, Samo avdio, Ableton Link
- **Pro nacin:** 8 pravil z izbiro svetila, kanala, frekvencnega pasu in krivulje odziva
- Persistenca: binarna shema V6 z obratno zdruzljivostjo (V1-V6)

### 7. LFO Engine (`lfo_engine.h/cpp`)
- Do 8 LFO instanc
- 4 valovne oblike: Sine, Triangle, Square, Sawtooth
- 6 ciljev: Dimmer, Pan, Tilt, R, G, B
- Fazni razmik med svetili in simetrija
- Hitrost 0.1 - 10.0 Hz z Master Speed mnozilnikom

### 8. Shape Generator (`shape_engine.h/cpp`)
- Do 4 oblik hkrati
- 6 tipov: Circle, Figure-8, Triangle, Square, Line-H, Line-V
- Pan/Tilt depth in fazni razmik med svetili
- Namenjen za moving head svetila (avtomatske gibalne sekvence)

### 9. Pixel Mapper (`pixel_mapper.h/cpp`) -- NOV
- **Samo ESP32-S3** (pogojna kompilacija z `CONFIG_IDF_TARGET_ESP32S3`)
- Krmili zunanji WS2812/NeoPixel LED trak prek **RMT periferne enote** (zero CPU load)
- GPIO: `PIXEL_STRIP_PIN = 16`, do `PIXEL_MAX_LEDS = 144` LED-ic
- Adafruit_NeoPixel knjiznica, kazalec shranjen kot `void*` za izogib headerski odvisnosti
- **4 nacini delovanja:**
  - **PXMAP_FIXTURE (Fixture Mirror):** preslikava RGB barv iz izbrane fixture ali skupine na LED trak, z dimmer aplikacijo
  - **PXMAP_VU_METER:** klasicni VU meter (zelena -> rumena -> rdeca) iz avdio nivoja
  - **PXMAP_SPECTRUM:** FFT spektrum — vsak LED = 1 frekvencni bin, mavricna barva (low=rdeca -> high=vijolicna)
  - **PXMAP_PULSE (Beat Pulse):** vsi LED-i utripajo na zaznani beat s pocasno rotacijo barve (~30 stopinj/s)
- Konfiguracija: enabled, ledCount, brightness, mode, fixtureIdx, groupMask
- Persistenca: binarna datoteka `/pixmap.bin` (verzija 1)
- Reinicializacija ob spremembi stevila LED-ic ali omogocitve

### 10. Ableton Link (`link_beat.h/cpp`) -- NOV
- **Pogojna kompilacija** z `__has_include(<ableton/Link.h>)`:
  - Ce je knjiznica nalozena: polna implementacija z UDP multicast sinhronizacijo
  - Ce knjiznica NI nalozena: stub nacin (vse metode vrnejo privzete vrednosti, brez napak pri kompilaciji)
- **Sinhronizacija:**
  - BPM: bere se iz Link seje, posodobi `_mbCfg.bpm`
  - Beat phase (0.0-1.0): fazna pozicija znotraj trenutnega beata
  - Bar phase (0.0-1.0): fazna pozicija znotraj takta (4 beati)
  - Beat trigger: zaznava ko se faza ovije (prehod > 0.5 na < prevBeatPhase)
- Peer discovery: stevilo povezanih DJ programov (Traktor, Rekordbox, Ableton Live)
- Enable/disable dinamicno prek web UI
- Quantum = 4 beati na takt

### 11. ESP-NOW Wireless DMX (`espnow_dmx.h/cpp`) -- NOV
- **Peer-to-peer brezvicni DMX** z <1ms latenco (brez usmerjevalnika/AP)
- Slave sprejemnik: poceni ESP32 + MAX485 = brezvicni DMX oddajnik (~3 EUR)
- **Fragmentacija DMX bufferja:**
  - 512B DMX buffer se razdeli v 3 pakete po 200B (ESP-NOW limit = 250B)
  - Format paketa: `[SEQ:1][OFFSET_HI:1][OFFSET_LO:1][LEN:1][DATA:do 200]`
  - Sekvenencna stevilka za zaznavo izgubljenih paketov
- Do **4 slave sprejemniki** (ESPNOW_MAX_PEERS)
- Broadcast na vse registrirane pere hkrati (`esp_now_send(NULL, ...)`)
- Dinamicno dodajanje/odstranjevanje perov prek web UI
- Persistenca: binarna datoteka `/espnow.bin` (verzija 1)
- Init/deinit ESP-NOW brez ponovnega zagona

### 12. sACN Output (`sacn_output.h/cpp`)
- E1.31 sACN multicast UDP oddajanje na port 5568
- Podpora za dinamicno nastavljanje univerze
- Aktivira se samo v lokalnem nacinu (preprecitev feedback loopa z ArtNet)

### 13. OSC Server (`osc_server.h/cpp`)
- OSC protokol na UDP portu 8000
- Dispatch mehanizem za zunanje kontrolerje (TouchOSC, Lemur...)
- Parsiranje OSC sporocil s float in int32 argumenti

### 14. Persona vmesniki (`personas/`, `persona-core.js`) -- NOV
- **6 poenostavljenih spletnih vmesnikov** za razlicne uporabnike + portal za izbiro
- Serviranje iz LittleFS prek `serveGzFile()` v `web_ui.cpp` (gzip + Content-Encoding header)
- **persona-core.js** (~115 vrstic): skupna JS knjiznica z WebSocket reconnect, status callbacks, config/scenes/cues API
- **PWA manifest** za vsako persono — `display: standalone` za fullscreen brez HTTPS
- **Persona config API:** `GET/POST /api/persona-cfg` -> `/persona.json` na LittleFS
- **Persone:**
  - **staff.html** (~6 KB): veliki gumbi za scene presets, master dimmer, blackout
  - **sound-eng.html** (~12 KB): scene grid, STL toggle, env preseti, skupinski dimmerji, FFT
  - **theater.html** (~10 KB): GO/BACK/STOP, cue list prikaz, crossfade progress, house lights
  - **dj.html** (~14 KB): BPM display, tap tempo, mood preseti, beat programi, Link info
  - **event.html** (~16 KB): timeline s fazami, auto-advance (client-side timer), Wake Lock API
  - **busker.html** (~8 KB): FFT vizualizacija, STL toggle, 4 look preseti
- **Build:** `build_personas.sh` gzipa HTML/JS, kopira manifeste, generira placeholder ikone
- **Secure Context Wizard:** vodic za Chrome Android flags bypass (DeviceOrientation brez HTTPS)

### 15. Web UI (`web_ui.h/cpp`, `index.html`) -- POSODOBLJEN
- **AsyncWebServer** na portu 80 z gzip strezi HTML-ja (~93KB stisnjen)
- **WebSocket** (`/ws`) za posodobitve v realnem casu (~12fps)
- **Multiplayer sinhronizacija:** `_forceSendState` ob novi WebSocket povezavi poslje celotno stanje
- **HSV -> RGBW konverzija na ESP32** (ukaz `cmd:"hue"`):
  - En WebSocket ukaz namesto 6-8 posamicnih kanalov
  - Avtomatska detekcija W/A/UV kanalov in izracun izhodov
- **Pixel Mapper ukazi:** `px_cfg` (nastavi konfiguracijo), `px_save` (shrani)
- **ESP-NOW ukazi:** `now_en` (vklop/izklop), `now_add` (dodaj peer), `now_rm` (odstrani), `now_save` (shrani)
- **Ableton Link:** avtomatski vklop ob izbiri BSRC_LINK, status broadcast (peers, BPM, connected)
- REST API za vse operacije
- HTTP Basic Auth opcija
- **Persona route:** 7 persona HTML + shared JS + 7 PWA manifestov + ikone iz LittleFS
- **Persona config API:** `GET/POST /api/persona-cfg` za urejanje persona konfiguracije
- **Secure Context Wizard** v XY Pad popupu za Chrome Android DeviceOrientation bypass

#### Web UI zavihki (7+)
| Zavihek | Vsebina |
|---------|---------|
| **Mixer** | Faderji za DMX kanale, fixture kontrole, XY pad za Pan/Tilt, barvni izbirnik, HSV kolut |
| **Scene** | Shranjevanje/nalaganje 20 scen, crossfade, cue list (40 cue-jev) |
| **Sound** | Easy/Pro/Manual beat nacin, FFT vizualizacija, AGC, preseti, beat programi |
| **Fixtures** | Patch management, profili, skupine, fixture nastavitve |
| **Nastavitve** | WiFi, ArtNet, audio vhod, LFO, Shape, **Gamepad**, **Pixel Mapper**, **ESP-NOW** |
| **Konfig** | Hostname, univerza, avtentikacija, import/export, OTA, **persona konfiguracija** |
| **DMX Mon** | DMX monitor v realnem casu (512 kanalov) |

#### Nove Web UI funkcionalnosti
- **Magic Wand (Carobna palicka):** DeviceOrientation API za uporabo telefona kot giroskopa
  - Nagib telefona krmili Pan/Tilt svetil v realnem casu
  - Zahteva `DeviceOrientationEvent.requestPermission()` na iOS
  - Aktivacija z gumbom "Wand" v XY pad kartici
- **Gamepad API:** podpora za PlayStation in Xbox kontrolerje
  - Palic -> Pan/Tilt, gumbi za blackout/flash/scene recall
  - `requestAnimationFrame` polling zanke
  - Samodejno zaznavanje prikljucitve/odkljucitve
- **Pixel Mapper nastavitve:** konfiguracija LED traku (stevilo LED, svetlost, nacin, fixture preslikava)
- **ESP-NOW nastavitve:** vklop/izklop, dodajanje/brisanje slave perov po MAC naslovu
- **Ableton Link izbira:** selektor vira beata vkljucuje "Ableton Link", statusni prikaz (stevilo perov, BPM)
- **2D Layout Editor** za pozicioniranje svetil na odru
- **Celozaslonski nacin** konzole

---

## Stavni avtomat (ControlMode)

```
ARTNET ----(10s timeout)----> LOCAL_AUTO ----(ArtNet se vrne)----> ARTNET
                                   |
                            (rocni preklop)
                                   v
                             LOCAL_MANUAL ----(rocni preklop nazaj)----> ARTNET
                                   |
                            (primaryMode)
                                   v
                            LOCAL_PRIMARY (ArtNet se ignorira, samo obvesti)
```

Prehod med nacini vsebuje 1-sekundno mehko prehajanje (mode crossfade) z linearno interpolacijo.

---

## Strojne povezave (GPIO)

### ESP32 DevKit V1

| Funkcija | GPIO | Namen |
|----------|------|-------|
| DMX TX | 17 | UART2 oddajanje na MAX485 |
| DMX DE | 4 | MAX485 driver enable |
| RGB LED R/G/B | 25/26/27 | PWM indikacija stanja |
| Reset gumb | 14 | Factory reset (3s ob zagonu) |
| I2S Line BCK/WS/DATA | 32/33/36 | WM8782S ADC (line-in) |
| I2S Mic BCK/WS/DATA | 22/21/34 | INMP441 mikrofon |

### ESP32-S3 N16R8

| Funkcija | GPIO | Namen |
|----------|------|-------|
| DMX TX | 17 | UART1 oddajanje na MAX485 |
| DMX DE | 8 | MAX485 driver enable |
| NeoPixel (vgrajen) | 48 | WS2812 RGB LED na ploscici |
| Pixel Mapper (zunanji) | 16 | WS2812 LED trak (RMT) |
| Reset gumb | 14 | Factory reset (3s ob zagonu) |
| I2S Line BCK/WS/DATA | 4/5/6 | WM8782S ADC (line-in) |
| I2S Mic BCK/WS/DATA | 12/11/10 | INMP441 mikrofon |

> GPIO 26-37 NI na voljo na ESP32-S3 N16R8 (Octal SPI flash/PSRAM).

### LED indikacija stanja
- **Zelena (utripa)** -- ArtNet aktiven, paketi prihajajo
- **Zelena (stalna)** -- ArtNet aktiven, brez nedavnih paketov
- **Cyan** -- Lokalni nacin (samodejni timeout)
- **Magenta** -- Lokalni nacin (rocni preklop)
- **Modra** -- PRIMARY lokalni nacin
- **Modra (utripa)** -- WiFi povezovanje
- **Rumena** -- AP nacin
- **Rdeca (utripa)** -- Safe mode (po vec crashih)
- **Rdeca** -- Factory reset

---

## Knjiznice

| Knjiznica | Namen | Opombe |
|-----------|-------|--------|
| **ArtnetWifi** | ArtNet protokol (UDP port 6454) | Nathanael Lecaude |
| **ESPAsyncWebServer** | Asinhroni web streznik | mathieucarbou fork za ESP32 core 3.x |
| **AsyncTCP** | Asinhroni TCP | mathieucarbou fork |
| **ArduinoJson 7.x** | JSON razclnjevanje in generiranje | Brez staticnega bufferja (JsonDocument) |
| **ESP-DSP** | Hardware-accelerated FFT | Vgrajen v ESP32 Arduino core, Vector ISA na S3 |
| **Adafruit_NeoPixel** | WS2812 LED krmiljenje | Samo ESP32-S3, RMT periferna enota |
| **LittleFS** | Flash datotecni sistem | Vgrajen v ESP32 core, samopopravilo |
| **esp_now** | ESP-NOW peer-to-peer WiFi | Vgrajen v ESP-IDF |
| **ESPmDNS** | mDNS za hostname.local | Vgrajen v ESP32 core |
| **Ableton Link** | BPM/beat grid sinhronizacija | Opcijska, pogojna kompilacija |

---

## Varnost in stabilnost

- **Watchdog:** 8s timeout, po 3 zaporednih WDT resetih --> **varni nacin** (brez FFT/zvoka)
  - WDT reset stevec v RTC pomnilniku (`RTC_NOINIT_ATTR`) prezivi soft-reset
  - Normalen zagon ponastavi stevec
- **Factory reset:** GPIO14 drzan 3s ob zagonu --> format LittleFS + ponovni zagon
- **WiFi failover:** do 5 omrezij s prioritetnim vrstnim redom, padec na AP nacin (`ARTNET-NODE` / `artnetdmx`)
- **LittleFS:** samopopravilo ob poskodbi, `LittleFS.begin(true)` za avtomatski format
- **FreeRTOS mutex:** za thread-safe dostop do DMX medpomnilnikov med jedroma
- **ArtNet feedback loop preprecitev:** ArtNet OUT se ne oddaja ko je nacin CTRL_ARTNET
- **Binarna persistenca z verzioniranjem:** V1-V4 shema za zvocne nastavitve, obratna zdruzljivost

---

## Poraba virov

### RAM -- ESP32 DevKit V1 (~120-150 KB od 320 KB DRAM)
| Komponenta | Velikost |
|------------|----------|
| WiFi sklad | ~50 KB |
| AsyncWebServer + WS | ~15 KB |
| DMX medpomnilniki (3 x 512 + crossfade bufferji) | ~4 KB |
| Profili svetil (24 slotov) | ~14 KB |
| Scene (20 x 536B) | ~10.7 KB |
| FFT medpomnilniki (512 x float x 2 + window) | ~6 KB |
| Beat/Sound nastavitve | ~2 KB |
| FreeRTOS jedro + naloge | ~24 KB |
| **Prosta rezerva** | **~190 KB** |

### RAM -- ESP32-S3 N16R8 (320KB DRAM + 8MB PSRAM)
| Komponenta | Lokacija | Velikost |
|------------|----------|----------|
| FFT interleaved buffer (1024 x 2 x float) | PSRAM | ~8 KB |
| Pixel Mapper (Adafruit_NeoPixel, 144 LED x 3B) | DRAM | ~0.5 KB |
| ESP-NOW bufferji | DRAM | ~1 KB |
| Ableton Link objekt | DRAM | ~2 KB |
| auxTask sklad | DRAM | 8 KB (namesto 4 KB) |
| PSRAM skupaj na voljo | PSRAM | **~8 MB** |

### LittleFS
| Vsebina | Velikost |
|---------|----------|
| 29 profilov svetil (JSON) | ~50 KB |
| 20 scen (binarne) | ~10.7 KB |
| Konfiguracija (config/patch/groups JSON) | ~2 KB |
| Zvocne nastavitve (sound.bin, V6) | ~1 KB |
| Pixel Mapper nastavitve (pixmap.bin) | <1 KB |
| ESP-NOW nastavitve (espnow.bin) | <1 KB |
| Persona datoteke (/p/) | ~35 KB |
| Persona konfiguracija (persona.json) | ~1 KB |
| **Skupaj ESP32:** | ~65 KB od 2 MB (brez person) |
| **Skupaj ESP32-S3:** | ~100 KB od ~9.9 MB (s personami) |

### Particijska tabela (ESP32-S3, partitions.csv)
| Particija | Tip | Velikost |
|-----------|-----|----------|
| app0 | OTA_0 | 3 MB |
| app1 | OTA_1 | 3 MB |
| spiffs | LittleFS | ~9.9 MB |
| nvs | NVS | 20 KB |
| coredump | Coredump | 64 KB |

---

## Zagonska sekvenca

1. **Sprostitev BT pomnilnika** (BLE na S3, BT+BLE na ESP32 -- prihranek ~64-70KB)
2. **Serial inicializacija** (115200 baud, izpis verzije in platforme)
3. **PSRAM preverba** (S3: izpis velikosti, opozorilo ce ni zaznan)
4. **Watchdog preverba** -- stevec resetov, aktivacija safe mode po 3 crashih
5. **LED inicializacija** (modra ali rdeca v safe mode)
6. **Factory reset preverba** (GPIO14 drzan 3s)
7. **Montaza LittleFS** (samopopravilo ob poskodbi, avtomatski format)
8. **Namestitev privzetih profilov** (3 osnovni ob prvem zagonu)
9. **Nalaganje konfiguracije** iz `/config.json`
10. **WiFi nastavitev** (preizkus vseh AP, padec na AP nacin)
11. **mDNS zagon** (`hostname.local`, storitve: artnet/udp, http/tcp)
12. **DMX output inicializacija** (UART z GPIO konfiguracijami za platformo)
13. **Inicializacija motorjev:** Fixture --> Scene --> Mixer --> Sound (preskok v safe mode)
14. **Audio vhod** (ce je nastavljen in ni safe mode)
15. **ArtNet poslusanje** + ArtPollReply UDP
16. **Web streznik** na portu 80 z WebSocket
17. **LFO Engine** + povezava z mixer in web UI
18. **Shape Generator** + povezava z mixer in web UI
19. **sACN output** (ce je omogocen v konfiguraciji)
20. **Pixel Mapper inicializacija** (samo S3) + povezava z web UI
21. **ESP-NOW inicializacija** + povezava z web UI
22. **OSC streznik** na portu 8000
23. **ArtPollReply** posiljanje ob zagonu
24. **Izpis pomnilnika** (prosti heap, interni heap)
25. **Watchdog rekonfiguracija** (8s timeout, panic = true)
26. **Ustvarjanje auxTask na Core 0** (8KB stack s PSRAM, 4KB brez)
27. **Izpis koncane inicializacije**

---

## Komunikacijski protokoli

### Vhod
| Protokol | Port | Opis |
|----------|------|------|
| ArtNet (Art-Net III) | UDP 6454 | DMX vnos iz razsvetljavalnih programov |
| OSC | UDP 8000 | Zunanji kontrolerji (TouchOSC, Lemur) |
| WebSocket | TCP 80 /ws | Web UI dvosmerna komunikacija |
| HTTP REST | TCP 80 | Konfiguracija, profili, scene |
| Ableton Link | UDP multicast | BPM/beat grid sinhronizacija |

### Izhod
| Protokol | Medij | Opis |
|----------|-------|------|
| DMX512 | UART (zicni) | MAX485 pretvornik, 40 fps |
| ArtNet OUT | UDP broadcast | DMX kot ArtNet (samo v lokalnem nacinu) |
| sACN (E1.31) | UDP multicast 5568 | DMX kot sACN (samo v lokalnem nacinu) |
| ESP-NOW | WiFi peer-to-peer | Brezvicni DMX na slave sprejemnike |
| WS2812 | RMT (GPIO) | Pixel Mapper LED trak (ESP32-S3) |
| ArtPollReply | UDP broadcast 6454 | Samodejno vsake 5s za discovery |

---

## WebSocket ukazi (cmd)

### Mixer kontrola
| Ukaz | Parametri | Opis |
|------|-----------|------|
| `ch` | a, v | Nastavi DMX kanal (1-512) |
| `fxch` | f, c, v | Nastavi fixture kanal |
| `grch` | g, c, v | Nastavi group kanal |
| `hue` | f, h, s, v | HSV -> RGBW konverzija za fixture |

### Pixel Mapper
| Ukaz | Parametri | Opis |
|------|-----------|------|
| `px_cfg` | enabled, ledCount, brightness, mode, fixtureIdx, groupMask | Nastavi konfiguracijo |
| `px_save` | / | Shrani na LittleFS |

### ESP-NOW
| Ukaz | Parametri | Opis |
|------|-----------|------|
| `now_en` | v (0/1) | Vklop/izklop ESP-NOW |
| `now_add` | mac, name | Dodaj slave peer |
| `now_rm` | idx | Odstrani peer |
| `now_save` | / | Shrani na LittleFS |

---

## Omejitve

| Omejitev | Opis |
|----------|------|
| 1 ArtNet univerza | 512 kanalov (razsiritev na vec univerz ni implementirana) |
| Brez RDM | Samo oddajanje, brez povratnega kanala |
| FFT 512/1024 vzorcev | Kompromis RAM vs resolucija (odvisno od PSRAM) |
| Max 24 svetil | Omejitev za odzivnost web UI |
| Pixel Mapper samo ESP32-S3 | Zahteva RMT in dovolj GPIO |
| Ableton Link opcijski | Zahteva namestitev zunanje knjiznice |
| ESP-NOW max 4 perov | Omejitev za zanesljiv broadcast |
| Crossfade med nacini 1s | Ni nastavljivo |
| ESP-NOW brez sifriranja | `encrypt = false` za hitrost |

---

## Ocena kakovosti kode

### Pozitivno
- **Modularna arhitektura** z jasno locenimi odgovornostmi (18 parov .h/.cpp)
- **Ucinkovita uporaba obeh jeder** ESP32 z jasno razmejenim delom: realtimno (Core 1) in racunsko intenzivno (Core 0)
- **Platformska abstrakcija** v `config.h` z `#if defined(CONFIG_IDF_TARGET_ESP32S3)` za podporo obeh platform
- **Hardware FFT** z ESP-DSP knjiznico namesto softverske ArduinoFFT -- znatno hitrejse na ESP32-S3
- **Pre-computed Hamming okno** eliminira ponavljajoce se izracune trigonometricnih funkcij
- **PSRAM-aware alokacija** z `psramPreferMalloc()` helper funkcijo
- **AGC (Automatic Gain Control)** za avtomatsko prilagajanje glasnosti z nastavljivim per-band gain
- **Pogojna kompilacija** za Ableton Link (`__has_include`) omogoca kompilacijo brez zunanje knjiznice
- **void* za NeoPixel kazalec** preprecuje sirjenje headerske odvisnosti v .h datoteko
- **Binarna persistenca z verzioniranjem** (V1-V4) za obratno zdruzljivost zvocnih nastavitev
- **Fiksna aritmetika** v casovno kriticnih poteh (crossfade alpha 0-256)
- **ESP-NOW fragmentacija** s sekvenencno stevilko za zaznavo izgubljenih paketov
- **Dobro definirani profili svetil** s specifikacijo (PROFILE_SPEC.md)
- **29 prednalosenih profilov** za takojsnjo uporabo
- **Mehko prehajanje** med nacini preprecuje motnje v razsvetljavi
- **Thread safety** z FreeRTOS mutex za kriticne odseke
- **Watchdog z varnim nacinom** za odpornost na crashe v produkciji
- **Obsezna dokumentacija** v slovenscini (6 dokumentacijskih datotek)

### Mozne izboljsave
- `web_ui.cpp` (~60KB, ~1400 vrstic) in `index.html` (~5500 vrstic) sta zelo obsezni -- razdelitev na manjse module bi izboljsala preglednost
- `web_ui_gz.h` (~373KB stisnjen HTML) otezuje pregled web vmesnika v repozitoriju
- `sound_engine.cpp` (~1080 vrstic) vsebuje vso logiko vseh beat programov v eni metodi -- razbijanje v locen modul bi pomagalo
- `void*` pristop za NeoPixel v pixel_mapper.h je funkcionalen, a tezje berljiv kot forward deklaracija s templatizirano delokacijo
- ESP-NOW ne uporablja sifriranja (`encrypt = false`) -- za produkcijsko uporabo bi bilo priporocljivo dodati
- Nekatere magicne stevilke v kodi bi lahko bile poimenovane konstante (npr. `0.02f` prag tisine v sound engine)
- Program chain in group beat override dodajata znacilno kompleksnost v `applyManualBeatProgram()` (~200 vrstic)
- Razmisliti o podpori za vec ArtNet univerz (multi-universe)
- OTA posodabljanje: particijska tabela podpira OTA (app0/app1), a implementacija v web UI je omejena

---

## Povzetek

To je profesionalno zasnovan ESP32/ESP32-S3 sistem za upravljanje razsvetljave, ki pokriva celoten spekter od mreznega ArtNet/sACN sprejemanja, prek lokalnega mesanja s scenami in crossfade, do zvocno reaktivne razsvetljave z hardware FFT analizo. Novejse funkcionalnosti vkljucujejo **Ableton Link sinhronizacijo** za DJ-sko integracijo, **ESP-NOW brezvicni DMX** za poceni razsiritev dosega, in **Pixel Mapper** za krmiljenje addressable LED trakov.

Koda je organizirana v 18+ modulov z jasno locenimi odgovornostmi, podpira dve strojni platformi (ESP32 in ESP32-S3) z eno kodno bazo, in sledi industrijskim standardom (DMX512, ArtNet III, sACN E1.31, Ableton Link) ob ucinkoviti uporabi omejenih virov ESP32. Web vmesnik s 7+ zavihki, podporo za giroskop telefona (Magic Wand) in Gamepad API, nudi polno kontrolo brez namescanja dodatne programske opreme. **Persona vmesniki** nudijo poenostavljene spletne strani za razlicne uporabnike (tonski mojster, gledalisce, DJ, osebje, organizator prireditev, ulicni performer) — servirane iz LittleFS s PWA podporo za fullscreen prikaz na mobilnih napravah.
