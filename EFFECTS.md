# Efekti — Uporabniški priročnik

Efekti (LFO, Shape Generator, Master Speed) se nahajajo na zavihku **Sound**, pod razdelki za zvočne nastavitve.

---

## 1. LFO / FX Generator

Nizkofrekvenčni oscilator — ustvarja ponavljajoče vzorce na DMX kanalih **neodvisno od zvoka**. Lahko deluje hkrati z Easy Mode in Manual Beat.

### Dodajanje LFO-ja

1. Izberi **valovno obliko** (Waveform)
2. Izberi **cilj** (Target) — na kateri DMX parameter vpliva
3. Nastavi **Rate** (hitrost), **Depth** (globina) in **Phase** (fazni razpored)
4. Izberi **simetrijo** (Symmetry) za vzorec faznega razporeda
5. Označi fixture-e, ki jih bo LFO poganjal
6. Klikni **"+ Dodaj LFO"**

### Valovne oblike

| Oblika   | Opis                                    | Uporaba                         |
|----------|------------------------------------------|---------------------------------|
| Sine     | Gladka sinusoida                        | Dihanje, počasno valovanje      |
| Triangle | Linearni ramp gor/dol                   | Enakomerna oscilacija           |
| Square   | Ostra vklop/izklop                      | Utripanje, on/off efekt         |
| Saw      | Žagast val (postopen dvig, hiter padec) | Gradnja intenzitete             |

### Cilji (Targets)

- **Dimmer** — modulira svetilnost
- **Pan** — horizontalno gibanje (16-bit za natančnost)
- **Tilt** — vertikalno gibanje (16-bit za natančnost)
- **Red / Green / Blue** — barvna modulacija

### Parametri

- **Rate** (0.1–10 Hz): Hitrost oscilacije. 0.5 = en cikel na 2 sekundi, 2.0 = dva cikla na sekundo
- **Depth** (0–1): Amplituda. 0.5 = nihanje ±50% od bazne vrednosti faderja
- **Phase** (0–1): Fazni zamik med fixture-i. 0 = vsi synkrono, 0.25 = vsak fixture zamaknjen za četrtino → val potuje čez fixture-e

### Simetrija (Symmetry)

Določa, kako se fazni zamik razporedi med fixture-i:

| Simetrija   | Opis                                      | Vizualni efekt                  |
|-------------|--------------------------------------------|---------------------------------|
| → Naprej    | 1→2→3→4 — normalno                       | Val potuje od prvega do zadnjega|
| ← Nazaj     | 4→3→2→1 — obratno                        | Val potuje od zadnjega do prvega|
| ↔ Sredina   | Iz sredine navzven (center-out)           | Val se širi od sredine navzven  |
| ⇄ Zunaj     | Od zunaj proti sredini (ends-in)          | Val se steka od robov proti centru|

### Primeri

**Dihanje svetlobe:**
Sine, Dimmer, Rate 0.3, Depth 0.8, Phase 0, Symmetry Naprej → vsi fixture-i se sinhrono dvigujejo in spuščajo.

**Val intenzitete:**
Sine, Dimmer, Rate 0.5, Depth 1.0, Phase 0.25, Symmetry Naprej → val svetlobe potuje od prvega do zadnjega fixture-a.

**Zrcalni val:**
Sine, Pan, Rate 0.3, Depth 0.5, Phase 0.5, Symmetry Sredina → moving headi se gibljejo iz centra navzven.

### Upravljanje

- Do **8 aktivnih LFO-jev** hkrati
- Klikni X ob LFO-ju za brisanje posameznega
- **"Zbriši vse"** — pobriše vse LFO-je naenkrat

---

## 2. Shape Generator

Ustvarja gibalne vzorce za **Pan/Tilt** na moving head lučeh. Shape generator avtomatično krmili Pan in Tilt kanale za ustvarjanje gibalnih vzorcev v prostoru.

### Oblike

| Oblika     | Opis                     |
|-----------|--------------------------|
| Circle    | Krožno gibanje            |
| Figure-8  | Osmička                  |
| Triangle  | Trikotniška pot          |
| Square    | Kvadratna pot            |
| Line H    | Vodoravna linija         |
| Line V    | Navpična linija          |

### Parametri

- **Speed** (0.1–5 Hz): Hitrost gibanja
- **Size** (0–1): Velikost vzorca. 0.5 = srednja amplituda Pan/Tilt gibanja
- **Spread** (0–1): Fazni zamik med fixture-i. 0 = vsi synkrono, 0.5 = pol cikla zamik

### Primer — krožno gibanje s chasom

Shape: Circle, Speed 0.5, Size 50%, Spread 0.25 → moving headi se vrtijo v krogu, vsak zamaknjen za četrtino cikla.

---

## 3. Master Speed

Globalni množilnik hitrosti za vse efekte (LFO, Shape, Sound-to-Light).

- Drsnik: 10%–400% (0.1x–4.0x)
- Hitri gumbi: **0.5x** | **1.0x** | **2.0x**
- Uporabno za hitro prilagoditev tempa vseh efektov hkrati brez spreminjanja posameznih Rate vrednosti

---

## 4. Pixel Mapper (LED trak) — samo ESP32-S3

Pixel Mapper poganja zunanji **WS2812 / NeoPixel LED trak**, priključen na **GPIO 16** na ESP32-S3. Za prenos podatkov uporablja **RMT periferno enoto** — animacija LED traku teče v strojni opremi brez obremenitve procesorja.

> **Opomba:** Pixel Mapper je na voljo izključno na ploščah ESP32-S3. Na navadnem ESP32 ta funkcionalnost ni podprta.

### Načini delovanja

| Način | Opis | Uporaba |
|-------|------|---------|
| Fixture Mirror | Preslikaj RGB barve iz DMX fixture-ov na LED trak | Ambient osvetlitev, ki sledi glavni razsvetljavi |
| VU Meter | Avdio nivo meter: zelena → rumena → rdeča | Za DJ-evo mizo ali za občinstvo |
| Spectrum | 8 FFT pasov kot mavričnih stolpcev | Vizualizacija glasbe na traku |
| Beat Pulse | Vsi LEDi utripajo na beat z vrtečo barvo | Energičen efekt za plesišča |

---

### 4.1 Fixture Mirror

Preslikava RGB barv iz DMX fixture-ov neposredno na LED trak.

**Kako deluje:**

- Izberi fixture ali skupino fixture-ov
- LED trak se razdeli na **segmente** (po en segment za vsak fixture)
- Vsak segment prikazuje RGB barvo fixture-a, **množeno z dimmerjem**
- Barve se posodabljajo v realnem času — LED trak sledi vsaki spremembi na fixture-ih

**Primer:**
60 LEDov, 3 fixture-i → vsak fixture dobi **20 LEDov**. Če je fixture 1 rdeč na 50% dimmerju, prvih 20 LEDov sveti temno rdeče. Če je fixture 2 modra na 100%, naslednjih 20 LEDov sveti polno modro.

---

### 4.2 VU Meter

Vizualni merilnik glasnosti, ki bere skupni avdio nivo iz AudioInput modula.

**Kako deluje:**

- **Zeleni LEDi** = nizek nivo zvoka
- **Rumeni LEDi** = srednji nivo zvoka
- **Rdeči LEDi** = visok nivo zvoka
- Število prižganih LEDov je proporcionalno glasnosti — glasneje = več LEDov sveti
- Prehodi med barvami so gladki, od spodnjega roba traku navzgor

**Primer:**
Postavite LED trak **vertikalno** ob DJ mizo ali na steno. Pri tihi glasbi svetijo le spodnji zeleni LEDi. Ko glasba naraste, se prižigajo rumeni in rdeči LEDi navzgor — kot klasični VU meter na mešalni mizi.

---

### 4.3 Spectrum

8 FFT frekvenčnih pasov, prikazanih kot mavrični stolpci na LED traku.

**Frekvenčni pasovi:**

| # | Pas | Opis |
|---|-----|------|
| 1 | Sub | Najnižje frekvence (sub-bas) |
| 2 | Bass | Bas (kick, bas kitara) |
| 3 | Low | Nizke sredine |
| 4 | Mid | Srednje frekvence |
| 5 | Hi-Mid | Višje sredine |
| 6 | Presence | Prisotnost (vokali, podrobnosti) |
| 7 | Brilliance | Svetlost (harmoniki) |
| 8 | Air | Najvišje frekvence (zrak) |

**Kako deluje:**

- Vsak frekvenčni pas dobi svoj del traku z **mavično barvo** (rdeča → oranžna → rumena → zelena → cian → modra → vijolična → magenta)
- **Višina** (število prižganih LEDov v segmentu) = energija v tistem frekvenčnem pasu
- Svetlejši LEDi pomenijo glasnejši signal v tistem pasu

**Primer:**
144 LEDov → vsak pas dobi **18 LEDov**. Ko udari kick drum, se rdeči (Sub) in oranžni (Bass) stolpci dvignejo. Ko zaigra vokal, se modri (Presence) stolpci dvignejo. Rezultat je živi frekvenčni spekter glasbe.

---

### 4.4 Beat Pulse

Vsi LEDi na traku utripajo na zaznani beat z vrtečo mavično barvo.

**Kako deluje:**

- Ob vsakem zaznanem **beatu** se vsi LEDi prižgejo na polno svetilnost
- **Barva se vrti** čez mavično paleto z vsakim beatom (beat 1 = rdeča, beat 2 = oranžna, beat 3 = rumena, itd.)
- Intenziteta **pada eksponencialno** po beatu — hiter blisk, ki počasi ugasne
- Naslednji beat ponovno prižge vse LEDe v novi barvi

**Primer:**
LED trak pod DJ mizo **pulzira v barvah na vsak udarec** basa. Vsak beat prinese novo barvo, intenziteta hitro naraste in počasi pade — ustvari energičen, dinamičen efekt za plesišče.

---

### Nastavitve Pixel Mapper-ja

Vse nastavitve za Pixel Mapper se nahajajo v zavihku **Nastavitve** (Settings).

| Nastavitev | Razpon | Opis |
|------------|--------|------|
| Vklopi / Izklopi | On / Off | Aktivira ali deaktivira Pixel Mapper |
| Število LEDov | 1–144 | Število LEDov na priključenem traku |
| Svetilnost | 0–255 | Globalna svetilnost traku (0 = izklop, 255 = polna) |
| Način | Fixture Mirror / VU Meter / Spectrum / Beat Pulse | Izbira načina delovanja |
| Fixture / Skupina | (seznam) | Izbira fixture-a ali skupine (samo za Fixture Mirror način) |

Konfiguracija se **shrani na LittleFS** — po ponovnem zagonu ESP32-S3 se nastavitve ohranijo (persistenca).

---

### Primer uporabe — LED trak za DJ booth

1. Poveži **WS2812 trak (60 LEDov)** na GPIO 16
2. Odpri **Nastavitve → Pixel Mapper → Vklopi**
3. Nastavi **Število LEDov: 60**, **Svetilnost: 200**
4. Izberi **Način: Spectrum**
5. LED trak prikazuje **živi FFT spekter glasbe** — stolpci se dvigujejo in spuščajo v ritmu glasbe

### Primer uporabe — ambient osvetlitev za bar

1. Poveži **WS2812 trak (144 LEDov)** pod bar
2. Nastavi **Način: Fixture Mirror**, izberi skupino **"Bar luči"**
3. Barve na traku se ujemajo z glavno razsvetljavo — ko spremenite barvo na fixture-ih, se LED trak posodobi v realnem času
4. Vklopi **Easy Mode** s presetom **Rainbow** → trak sledi barvni rotaciji po vseh fixture-ih

---

### Vezava (priključitev LED traku)

```
ESP32-S3 GPIO 16 ──→ WS2812 DIN (podatkovna linija)
ESP32-S3 GND     ──→ WS2812 GND
Zunanji 5V napajalnik ──→ WS2812 VCC + GND
```

> **Opomba:** Za več kot 30 LEDov **uporabite zunanji 5V napajalnik**. Ne napajajte traku neposredno iz ESP32 — prevelik tok lahko poškoduje ploščo. Skupna masa (GND) med ESP32 in zunanjim napajalnikom je obvezna.
