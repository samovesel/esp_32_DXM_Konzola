# Pregled kode: ESP32 ArtNet/DMX Konzola

## Kaj je ta projekt?

To je **profesionalni sistem za upravljanje razsvetljave** na osnovi ESP32, ki združuje:

- **ArtNet → DMX512 pretvornik** (gateway) prek WiFi
- **Samostojno mešalno mizo** (mixer) z web vmesnikom
- **Zvočno reaktivno razsvetljavo** z FFT analizo in detekcijo udarca (beat detection)
- **Upravljanje profilov svetil** in scen s crossfade funkcijo

**Verzija:** 1.0.0
**Jezik:** C++ (Arduino/ESP-IDF za ESP32)
**Platforma:** ESP32 DevKit V1 (dual-core, 4MB Flash, 320KB RAM)

---

## Arhitektura (dvojedrna)

| Jedro | Naloge |
|-------|--------|
| **Core 1** (Arduino loop) | ArtNet branje UDP, mixer posodabljanje, DMX izhod (40 fps), WebSocket |
| **Core 0** (auxTask) | Audio vzorčenje (ADC/I2S), FFT obdelava, LED status, WiFi/TCP |

Nitna varnost je zagotovljena s FreeRTOS muteksom za DMX medpomnilnike.

---

## Struktura projekta

```
esp_32_DXM_Konzola/
├── esp32_artnet_dmx.ino          Glavni sketch (setup/loop, dvojedrni zagon)
├── config.h                       Konstante, strukture, enumeracije
├── config_store.h                 JSON persistenca (NodeConfig, Patch, Groups)
├── dmx_driver.h/cpp              UART DMX prenos (break + start code)
├── fixture_engine.h/cpp          Profili svetil + upravljanje patcha
├── mixer_engine.h/cpp            Stavni avtomat, mešanje kanalov, scene
├── scene_engine.h/cpp            Scene: shranjevanje/nalaganje, crossfade
├── audio_input.h/cpp             Audio vzorčenje (ADC/I2S), dvojno medpomnjenje
├── sound_engine.h/cpp            FFT, beat detekcija, zvočna pravila
├── led_status.h                   RGB LED indikacija stanja (PWM)
├── web_ui.h/cpp                  Web strežnik, REST API, WebSocket
├── web_ui_gz.h                   Stisnjen index.html (gzip)
├── index.html                     Web UI (5 zavihkov: Live, Fixtures, Mixer, Scenes, Sound)
├── data/
│   ├── profiles/                 Definicije svetil (29 JSON datotek)
│   └── scenes/                   Binarne scene (536B vsaka)
├── README.md                      Dokumentacija (slovenščina)
└── PROFILE_SPEC.md              Specifikacija profilov svetil v2.0
```

---

## Ključni moduli

### 1. DMX Driver (`dmx_driver.h/cpp`)
- UART2 prenos pri 250kHz, format 8N2
- Break signal: preklop na 96kHz za ~94µs LOW pulz
- Protokol: Start Code (0x00) + 512 DMX kanalov

### 2. Fixture Engine (`fixture_engine.h/cpp`)
- Nalaganje profilov svetil iz LittleFS `/profiles/` (JSON format)
- Podpora za večnačinske profile (npr. 4ch, 7ch, 16ch v eni datoteki)
- Do 16 svetil v patchu, 8 skupin (bitmask)
- 25 tipov kanalov: intensity, color (r/g/b/w/ww/a/uv/l/c), pan/tilt, strobe, gobo, prism, focus, zoom...

### 3. Mixer Engine (`mixer_engine.h/cpp`)
- Stavni avtomat: ARTNET ↔ LOCAL_AUTO ↔ LOCAL_MANUAL
- Dva DMX medpomnilnika: artnetShadow (vedno posodobljen) in manualValues (lokalne spremembe)
- Master dimmer, snapshoti (3 zadnja stanja za undo)
- 1-sekundno mehko prehajanje med načini

### 4. Scene Engine (`scene_engine.h/cpp`)
- Do 20 scen v LittleFS binarnem formatu (24B ime + 512B DMX = 536B)
- Crossfade z linearno interpolacijo (0-10 sekund)
- Fiksna aritmetika (alpha 0-256) za učinkovitost ESP32

### 5. Audio Input (`audio_input.h/cpp`)
- Dvojno medpomnjenje na Core 0
- ADC (GPIO36): 12-bit, 22050 Hz
- I2S (INMP441): 24-bit digitalni mikrofon

### 6. Sound Engine (`sound_engine.h/cpp`)
- FFT: 512 vzorcev, Hamming okno, resolucija ~43 Hz/bin
- 8 frekvenčnih pasov: sub-bas (30 Hz) do diskant (11 kHz)
- Beat detekcija: prag 1.4× drseče povprečje, min 200ms, BPM izračun
- **Easy način**: 5 prednastavitev (Pulse, Rainbow, Storm, Ambient, Club)
- **Pro način**: 8 pravil z izbiro svetila, kanala, frekvenčnega pasu in krivulje odziva

### 7. Web UI (`web_ui.h/cpp`, `index.html`)
- AsyncWebServer na portu 80
- WebSocket (`/ws`) za posodobitve v realnem času (~12fps)
- REST API za vse operacije
- 5 zavihkov: Live, Fixtures, Mixer, Scenes, Sound

---

## Stavni avtomat (ControlMode)

```
ARTNET ──(10s timeout)──→ LOCAL_AUTO ──(ArtNet se vrne)──→ ARTNET
                              │
                       (ročni preklop)
                              ↓
                        LOCAL_MANUAL ──(ročni preklop nazaj)──→ ARTNET
```

Prehod med načini vsebuje 1-sekundno mehko prehajanje (fade).

---

## Strojne povezave (GPIO)

| Funkcija | GPIO | Namen |
|----------|------|-------|
| DMX TX | 17 | UART2 oddajanje na MAX485 |
| DMX DE | 4 | MAX485 driver enable |
| RGB LED R/G/B | 25/26/27 | PWM indikacija stanja |
| Reset gumb | 14 | Factory reset (3s ob zagonu) |
| Audio line-in | 36 | ADC1_CH0 |
| I2S BCK/WS/DATA | 22/21/34 | INMP441 mikrofon |

### LED indikacija stanja
- **Zelena (utripa)** – ArtNet aktiven, paketi prihajajo
- **Zelena (stalna)** – ArtNet aktiven, brez nedavnih paketov
- **Cyan** – Lokalni način (samodejni timeout)
- **Magenta** – Lokalni način (ročni preklop)
- **Modra (utripa)** – WiFi povezovanje
- **Rumena** – AP način
- **Rdeča** – Factory reset ali varni način

---

## Knjižnice

- **ArtnetWifi** – ArtNet protokol
- **ESPAsyncWebServer** (mathieucarbou fork) – Asinhroni web strežnik
- **AsyncTCP** – Asinhroni TCP
- **ArduinoJson 7.x** – JSON razčlenjevanje
- **arduinoFFT** – Hitra Fourierova transformacija
- **LittleFS** – Flash datotečni sistem

---

## Varnost in stabilnost

- **Watchdog**: 8s timeout, po 3 zaporednih WDT resetih → **varni način** (brez FFT/zvoka)
- **Factory reset**: GPIO14 držan 3s ob zagonu → format LittleFS + ponovni zagon
- **WiFi failover**: do 5 omrežij, padec na AP način (`ARTNET-NODE` / `artnetdmx`)
- **LittleFS**: samopopravilo ob poškodbi

---

## Poraba virov

### RAM (~120-150 KB od 320 KB)
| Komponenta | Velikost |
|------------|----------|
| WiFi sklad | ~50 KB |
| AsyncWebServer + WS | ~15 KB |
| DMX medpomnilniki (3×512) | ~1.5 KB |
| Profili svetil | ~12 KB |
| Scene (5×512) | ~2.5 KB |
| FFT medpomnilniki (2×512×float) | ~4 KB |
| FreeRTOS jedro | ~24 KB |
| **Prosta rezerva** | **~200 KB** |

### LittleFS
- ~50 KB za 29 profilov svetil
- ~10.7 KB za 20 scen
- ~2 KB za konfiguracijo/patch/skupine

---

## Omejitve

| Omejitev | Opis |
|----------|------|
| 1 ArtNet universe | 512 kanalov |
| Brez RDM | Samo oddajanje, brez povratnega kanala |
| FFT 512 vzorcev | Kompromis RAM vs resolucija |
| Max 16 svetil | Omejitev za odzivnost web UI |
| Brez OTA | Posodabljanje samo prek USB |
| Crossfade med načini 1s | Ni nastavljivo |

---

## Ocena kakovosti kode

### Pozitivno
- Modularna struktura z jasno ločenimi odgovornostmi
- Učinkovita uporaba obeh jeder ESP32
- Fiksna aritmetika v časovno kritičnih poteh (crossfade)
- Dobro definirani profili svetil s specifikacijo (PROFILE_SPEC.md)
- Razumna uporaba RAM-a
- Mehko prehajanje med načini prepreči motnje v razsvetljavi
- 29 prednaloženih profilov za takojšnjo uporabo

### Možne izboljšave
- `web_ui.cpp` je zelo obsežna (~128KB) – razdelitev na manjše module bi izboljšala preglednost
- `web_ui_gz.h` (92.8KB stisnjen HTML) otežuje pregled web vmesnika v repozitoriju
- Dodati OTA posodabljanje za lažje vzdrževanje na terenu
- Nekatere magične številke v kodi bi lahko bile poimenovane konstante
- Razmisliti o podpori za več ArtNet univerz

---

## Zagonska sekvenca

1. Onemogočitev Bluetooth (prihranek 70KB)
2. Montaža LittleFS (samopopravilo ob poškodbi)
3. Namestitev privzetih profilov (3 osnovni ob prvem zagonu)
4. Nalaganje konfiguracije iz `/config.json`
5. WiFi nastavitev (preizkus vseh AP, padec na AP način)
6. Zagon mDNS (`hostname.local`)
7. Inicializacija motorjev: Fixture → Scene → Mixer → Sound
8. Zagon avdio vhoda (če je nastavljen)
9. Aktivacija ArtNet poslušalca
10. Zagon web strežnika na portu 80
11. Ustvarjanje dvojedrnih nalog (Core 0: auxTask)
12. Nastavitev watchdoga (8s timeout)

---

## Povzetek

To je profesionalno zasnovan ESP32 sistem za upravljanje razsvetljave, ki pokriva celoten spekter od mrežnega ArtNet sprejemanja, prek lokalnega mešanja s scenami in crossfade, do zvočno reaktivne razsvetljave z FFT analizo. Koda je dobro organizirana, dokumentirana v slovenščini, in sledi industrijskim standardom (DMX512, ArtNet) ob učinkoviti uporabi omejenih virov ESP32.
