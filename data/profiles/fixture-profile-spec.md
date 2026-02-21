# ESP32 ArtNet/DMX Node — Specifikacija fixture profilov (JSON)

## Namen
Ta specifikacija omogoča generiranje pravilnih JSON fixture profilov za ESP32 ArtNet/DMX node. Profil opisuje DMX kanale luči — imena, tipe, privzete vrednosti in opcijske obsege (ranges). Namenjena je za uporabo z LLM-ji, ki na podlagi opisa DMX kanalov generirajo pravilen JSON.

---

## Struktura JSON

```json
{
  "name": "<ime profila, max 27 znakov>",
  "channels": [
    {
      "name": "<ime kanala, max 19 znakov>",
      "type": "<tip kanala — glej tabelo>",
      "default": <privzeta vrednost 0-255>,
      "ranges": [
        { "from": <0-255>, "to": <0-255>, "label": "<opis, max 15 znakov>" }
      ]
    }
  ]
}
```

`ranges` je opcijsko polje — dodaj ga samo kadar ima kanal diskretne funkcije ali prelomne točke. Za linearne kanale (dimmer 0–100%, barva 0–100%) NE dodajaj ranges.

---

## Omejitve (KRITIČNO — naprava ima omejen RAM!)

| Polje | Omejitev | Posledica prekoračitve |
|---|---|---|
| `name` (profil) | **max 27 znakov** | Obreže se |
| `name` (kanal) | **max 19 znakov** | Obreže se |
| `channels` | **max 16 kanalov** na profil | Ignorira odvečne |
| `ranges` na kanal | **max 4 ranges** | Ignorira odvečne |
| `label` (range) | **max 15 znakov** | Obreže se |
| `default` | 0–255 | |
| `from`, `to` | 0–255, `from` ≤ `to` | |

### Pravilo za ranges > 4
Če ima kanal več kot 4 obsegov, **MORAŠ združiti** sorodne v širše bloke. Strategija:
1. Ohrani "Off" / "No function" / "Open" obseg
2. Združi sorodne funkcije v en blok (npr. 15 barv → `"Colors 1–15"`)
3. Ohrani funkcionalno najpomembnejše bloke
4. Uporabi kratke opise: `"Show 1–8"`, `"Gobo 1–4"`, `"2400→10800K"`

### Pravilo za kanale > 16
Če ima luč več kot 16 DMX kanalov, **izpusti** najmanj pomembne:
- Fine kanalov (pan fine, tilt fine, dimmer fine)
- Macro/reset kanale
- Podvojene funkcije
V imenu profila označi dejanski number kanalov, npr. `15ch`.

---

## Dovoljeni tipi kanalov (`type`)

**TOČEN niz** — naprava ga primerja s `strcmp()`, napačen type pomeni `generic`.

| `type` | Uporabi za | Barva v UI | `default` |
|---|---|---|---|
| `"intensity"` | Dimmer, master dimmer, brightness | Cyan | `0` |
| `"color_r"` | Red, intensity red | Rdeča | `0` |
| `"color_g"` | Green, intensity green | Zelena | `0` |
| `"color_b"` | Blue, intensity blue | Modra | `0` |
| `"color_w"` | White, cold white, warm white | Bela | `0` |
| `"color_a"` | Amber; tudi 2. beli kanal (warm white) če je cold white že `color_w` | Oranžna | `0` |
| `"color_uv"` | UV, ultraviolet | Vijolična | `0` |
| `"pan"` | Pan, rotation | Siva | `128` |
| `"pan_fine"` | Pan fine (16-bit LSB) | Siva | `0` |
| `"tilt"` | Tilt, inclination | Siva | `128` |
| `"tilt_fine"` | Tilt fine (16-bit LSB) | Siva | `0` |
| `"speed"` | Pan/Tilt speed, dimmer speed, running speed | Siva | `0` |
| `"strobe"` | Strobe, stroboscope, shutter (ko so v enem kanalu) | Rumena | `0` |
| `"shutter"` | Shutter (le če ločen od strobe) | Rumena | `0` |
| `"gobo"` | Gobo wheel | Rjava | `0` |
| `"prism"` | Prism | Siva | `0` |
| `"focus"` | Focus | Siva | `128` |
| `"zoom"` | Zoom | Siva | `0` |
| `"preset"` | Automatic shows, sound mode, operating mode | Siva | `0` |
| `"macro"` | Color macro, colour temp, color presets, fixed colour | Siva | `0` |
| `"generic"` | Karkoli kar ne spada zgoraj: lime, cyan, beam FX, segment, delay... | Cyan | `0` |

### Odločitvena pravila za tip:
1. Kanal se imenuje "Dimmer" ali "Master dimmer" → `"intensity"`
2. Kanal je barvna komponenta (Red/Green/Blue/White/Amber/UV) → ustrezna `"color_*"`
3. Kanal je "Lime" ali "Cyan" → `"generic"` (nimata svojega tipa)
4. Sta dva bela kanala (Cold White + Warm White) → prvi dobi `"color_w"`, drugi `"color_a"`
5. Kanal združuje strobe + shutter → `"strobe"`
6. Kanal je "Color temperature" ali "Colour macros" ali "Color presets" ali "Fixed colour" → `"macro"`
7. Kanal je "Automatic show" ali "Sound control" ali "Operating mode" → `"preset"`
8. Kanal je "Dimmer fine" → `"generic"` (ni dedikiranega tipa za dimmer fine)
9. Kanal je "Beam effects" ali "Segment pattern" ali "DMX Delay" → `"generic"`
10. Kadar dvomiš → `"generic"`

### Odločitvena pravila za default:
- **Pan, Tilt** → `128` (sredinska pozicija)
- **Focus** → `128`
- **Zoom** → `0` (razen če manual eksplicitno navaja middle = 128)
- **Vse ostalo** → `0`

---

## Pravila za `ranges`

### DODAJ ranges kadar:
- Kanal ima **diskretne funkcije** (gobo pozicije, preset programi, color macro)
- Kanal ima **prelomno točko** (npr. 0–9 = off, 10–255 = strobe speed)
- Kanal ima **več različnih efektov** v enem (shutter: open / strobe / random / pulse)

### NE dodajaj ranges kadar:
- Kanal je **linearni 0–100%** (dimmer, barva) — NI ranges
- Kanal je **fine** (16-bit LSB) — NI ranges
- Kanal je **linearni z opisom** (zoom 0%–100%, speed fast→slow) — NI ranges, razen če imaš break point

### Format in pravila:
```json
"ranges": [
  { "from": 0, "to": 9, "label": "Off" },
  { "from": 10, "to": 250, "label": "Slow → Fast" },
  { "from": 251, "to": 255, "label": "Open" }
]
```

- Ranges morajo **pokrivati celoten 0–255** brez praznin ali prekrivanj
- `from` prvega range-a = 0, `to` zadnjega = 255
- Labels: kratki! `→` za smerne opise, `–` za obsege, `K` za temperaturo
- Primeri dobrih labelov: `"Off"`, `"Open"`, `"Slow → Fast"`, `"Show 1–8"`, `"2400→6000K"`, `"Colors 1–35"`, `"CW"`, `"CCW"`, `"Sound ctrl"`

---

## Ime datoteke

Ime datoteke (brez `.json`) = **ID profila**. Pravila:
- **Male črke, številke, vezaji**: `par-rgbw-7ch.json`
- **Max 19 znakov** brez `.json`
- Priporočen format: `<tip>-<model>-<kanalov>ch.json`
- Primeri: `wash-340fx-16ch.json`, `par-720ip-9ch.json`, `spot-z100m-2ch.json`

---

## Primeri iz prakse

### Primer 1: Varytec LED Studio 150 (2 kanala)

**Vhod:**
```
Ch1: Master dimmer 0–255
Ch2: Stroboscope — 0–5 off, 6–255 strobe speed
```

**Izhod** (`spot-studio150-2ch.json`):
```json
{
  "name": "LED Studio 150 2ch",
  "channels": [
    { "name": "Dimmer", "type": "intensity", "default": 0 },
    { "name": "Strobe", "type": "strobe", "default": 0, "ranges": [
      { "from": 0, "to": 5, "label": "Off" },
      { "from": 6, "to": 255, "label": "Slow → Fast" }
    ]}
  ]
}
```

Razlaga: Dimmer je linearen → brez ranges. Strobe ima break point → 2 ranges.

---

### Primer 2: Stairville Z100M (2 kanala)

**Vhod:**
```
Ch1: Dimmer 0–255
Ch2: 0–15 open shutter, 16–95 strobe speed, 96–175 flash speed, 176–255 random
```

**Izhod** (`spot-z100m-2ch.json`):
```json
{
  "name": "Stairville Z100M 2ch",
  "channels": [
    { "name": "Dimmer", "type": "intensity", "default": 0 },
    { "name": "Strobe", "type": "strobe", "default": 0, "ranges": [
      { "from": 0, "to": 15, "label": "Open" },
      { "from": 16, "to": 95, "label": "Strobe" },
      { "from": 96, "to": 175, "label": "Flash" },
      { "from": 176, "to": 255, "label": "Random" }
    ]}
  ]
}
```

Razlaga: Strobe ima 4 različne sekcije → točno 4 ranges (maks!).

---

### Primer 3: Stairville Quad Par 5x8W RGBWW (8 kanalov)

**Vhod:**
```
Ch1: Dimmer 0–255
Ch2: Red 0–255 (if ch6=0)
Ch3: Green 0–255 (if ch6=0)
Ch4: Blue 0–255 (if ch6=0)
Ch5: Warm White 0–255 (if ch6=0)
Ch6: Operating mode — 0=manual, 1–24 fixed color, 25–249 shows, 250–255 sound
Ch7: Color select / Speed (depends on ch6)
Ch8: Strobe 0–255
```

**Izhod** (`par-quadpar-8ch.json`):
```json
{
  "name": "Quad Par 5x8W RGBWW 8ch",
  "channels": [
    { "name": "Dimmer", "type": "intensity", "default": 0 },
    { "name": "Red", "type": "color_r", "default": 0 },
    { "name": "Green", "type": "color_g", "default": 0 },
    { "name": "Blue", "type": "color_b", "default": 0 },
    { "name": "Warm White", "type": "color_w", "default": 0 },
    { "name": "Mode", "type": "preset", "default": 0, "ranges": [
      { "from": 0, "to": 0, "label": "Manual RGB" },
      { "from": 1, "to": 24, "label": "Fixed color" },
      { "from": 25, "to": 249, "label": "Show 2–10" },
      { "from": 250, "to": 255, "label": "Sound" }
    ]},
    { "name": "Color/Speed", "type": "macro", "default": 0 },
    { "name": "Strobe", "type": "strobe", "default": 0 }
  ]
}
```

Razlaga: Pogojni kanali (if ch6=0) → profil tega ne more izraziti, deluje pa na DMX nivoju. Strobe nima break point-a (linearen 0–255) → brez ranges. Mode ima 4 jasne sekcije → 4 ranges.

---

### Primer 4: Cameo ROOT Par 6 (12 kanalov, dimmer fine)

**Vhod:**
```
Ch1: Dimmer 0–255
Ch2: Dimmer fine 0–255
Ch3: Strobe functions — 0–5 open, 6–10 closed, 11–127 effects, 128–250 strobe, 251–255 open
Ch4: Red, Ch5: Green, Ch6: Blue, Ch7: White, Ch8: Amber, Ch9: UV
Ch10: Color presets/jumping/fading — 0–5 off, 6–125 colors, 126–127 stop, 128–201 FX, 202–255 user
Ch11: Sound — 0–5 off, 6–255 on
Ch12: DMX Delay — 0–5 off, 6–255 delay
```

**Izhod** (`par-rootpar6-12ch.json`):
```json
{
  "name": "Cameo ROOT Par 6 12ch",
  "channels": [
    { "name": "Dimmer", "type": "intensity", "default": 0 },
    { "name": "Dimmer fine", "type": "generic", "default": 0 },
    { "name": "Strobe", "type": "strobe", "default": 0, "ranges": [
      { "from": 0, "to": 5, "label": "Open" },
      { "from": 6, "to": 10, "label": "Closed" },
      { "from": 11, "to": 127, "label": "FX" },
      { "from": 128, "to": 255, "label": "Strobe" }
    ]},
    { "name": "Red", "type": "color_r", "default": 0 },
    { "name": "Green", "type": "color_g", "default": 0 },
    { "name": "Blue", "type": "color_b", "default": 0 },
    { "name": "White", "type": "color_w", "default": 0 },
    { "name": "Amber", "type": "color_a", "default": 0 },
    { "name": "UV", "type": "color_uv", "default": 0 },
    { "name": "Color Macro", "type": "macro", "default": 0, "ranges": [
      { "from": 0, "to": 5, "label": "Off" },
      { "from": 6, "to": 125, "label": "Colors" },
      { "from": 126, "to": 201, "label": "Jump/Fade" },
      { "from": 202, "to": 255, "label": "User colors" }
    ]},
    { "name": "Sound", "type": "preset", "default": 0, "ranges": [
      { "from": 0, "to": 5, "label": "Off" },
      { "from": 6, "to": 255, "label": "On" }
    ]},
    { "name": "DMX Delay", "type": "generic", "default": 0, "ranges": [
      { "from": 0, "to": 5, "label": "Off" },
      { "from": 6, "to": 255, "label": "0.1→2.0s" }
    ]}
  ]
}
```

Razlaga: Dimmer fine nima dedikiranega tipa → `"generic"`. Strobe ima 5 sekcij → združi zadnji dve (251–255 open z 128–250 strobe, ali pa strobe+open). Color macro ima 6+ sekcij → združi v 4. Sound je preprost on/off → 2 ranges.

---

### Primer 5: Varytec Hero Wash 340FX (16 kanalov, moving head)

**Vhod:**
```
Ch1: Pan 0–255 (mid=128), Ch2: Pan fine, Ch3: Tilt 0–255 (mid=128), Ch4: Tilt fine
Ch5: P/T Speed fast(0)→slow(255)
Ch6: Dimmer 0–255
Ch7: Shutter — 0–9 open, 10–250 strobe, 251–255 open
Ch8: Red, Ch9: Green, Ch10: Blue, Ch11: White
Ch12: Color temp — 0 off, 1–255 temperatures 2400K→10800K
Ch13: Color macros — 0–10 off, 11–150 colors, 151–200 change, 201–255 fade
Ch14: Zoom 0–255
Ch15: Beam FX — 0–100 open, 101–133 yo-yo, 134–194 CCW, 195–255 CW
Ch16: Auto — 0–10 off, 11–90 shows, 91–230 sound, 231–240 reset
```

**Izhod** (`wash-340fx-16ch.json`):
```json
{
  "name": "Hero Wash 340FX 16ch",
  "channels": [
    { "name": "Pan", "type": "pan", "default": 128 },
    { "name": "Pan fine", "type": "pan_fine", "default": 0 },
    { "name": "Tilt", "type": "tilt", "default": 128 },
    { "name": "Tilt fine", "type": "tilt_fine", "default": 0 },
    { "name": "P/T Speed", "type": "speed", "default": 0, "ranges": [
      { "from": 0, "to": 0, "label": "Max speed" },
      { "from": 1, "to": 255, "label": "Fast → Slow" }
    ]},
    { "name": "Dimmer", "type": "intensity", "default": 0 },
    { "name": "Shutter", "type": "strobe", "default": 0, "ranges": [
      { "from": 0, "to": 9, "label": "Open" },
      { "from": 10, "to": 250, "label": "Strobe" },
      { "from": 251, "to": 255, "label": "Open" }
    ]},
    { "name": "Red", "type": "color_r", "default": 0 },
    { "name": "Green", "type": "color_g", "default": 0 },
    { "name": "Blue", "type": "color_b", "default": 0 },
    { "name": "White", "type": "color_w", "default": 0 },
    { "name": "Color Temp", "type": "macro", "default": 0, "ranges": [
      { "from": 0, "to": 0, "label": "Off" },
      { "from": 1, "to": 127, "label": "2400→6000K" },
      { "from": 128, "to": 255, "label": "7200→10800K" }
    ]},
    { "name": "Color Macro", "type": "macro", "default": 0, "ranges": [
      { "from": 0, "to": 10, "label": "Off" },
      { "from": 11, "to": 150, "label": "Colors" },
      { "from": 151, "to": 200, "label": "Change" },
      { "from": 201, "to": 255, "label": "Fade" }
    ]},
    { "name": "Zoom", "type": "zoom", "default": 0 },
    { "name": "Beam FX", "type": "generic", "default": 0, "ranges": [
      { "from": 0, "to": 100, "label": "0°→90°" },
      { "from": 101, "to": 133, "label": "Yo-yo" },
      { "from": 134, "to": 194, "label": "CCW" },
      { "from": 195, "to": 255, "label": "CW" }
    ]},
    { "name": "Auto/Sound", "type": "preset", "default": 0, "ranges": [
      { "from": 0, "to": 10, "label": "Off" },
      { "from": 11, "to": 90, "label": "Show 1–8" },
      { "from": 91, "to": 230, "label": "Sound ctrl" },
      { "from": 231, "to": 255, "label": "Reset/Off" }
    ]}
  ]
}
```

Razlaga: Točno 16 kanalov — maks! Color temp in Color macro sta oba `"macro"` (legitimno, ista naprava ima dva kanala istega tipa). Auto/Sound ima 4 sekcije, zadnji dve (reset 231–240 + no function 241–255) sta združeni v `"Reset/Off"`.

---

### Primer 6: Varytec Hero Wash 300 TW (13 kanalov, brez pan/tilt fine)

**Vhod:**
```
Ch1: Pan (mid=128, NI fine), Ch2: Tilt (mid=128, NI fine), Ch3: P/T Speed
Ch4: Zoom 0–100%, Ch5: Dimmer
Ch6: Strobe — 0–10 off, 11–200 FX, 201–250 strobe, 251–255 on
Ch7: Cold white 0–100%, Ch8: Warm white 0–100%
Ch9: Color temp — 0–9 off, 10–255 = 2500→6100K
Ch10: Segment patterns (18 patterns)
Ch11: Segment speed
Ch12: Zoom auto programmes (8 programmes)
Ch13: Auto/Sound/Reset
```

**Izhod** (`wash-300tw-13ch.json`):
```json
{
  "name": "Hero Wash 300TW 13ch",
  "channels": [
    { "name": "Pan", "type": "pan", "default": 128 },
    { "name": "Tilt", "type": "tilt", "default": 128 },
    { "name": "P/T Speed", "type": "speed", "default": 0, "ranges": [
      { "from": 0, "to": 0, "label": "Max speed" },
      { "from": 1, "to": 255, "label": "Fast → Slow" }
    ]},
    { "name": "Zoom", "type": "zoom", "default": 0 },
    { "name": "Dimmer", "type": "intensity", "default": 0 },
    { "name": "Strobe", "type": "strobe", "default": 0, "ranges": [
      { "from": 0, "to": 10, "label": "Off" },
      { "from": 11, "to": 200, "label": "FX" },
      { "from": 201, "to": 250, "label": "Strobe" },
      { "from": 251, "to": 255, "label": "On" }
    ]},
    { "name": "Cold White", "type": "color_w", "default": 0 },
    { "name": "Warm White", "type": "color_a", "default": 0 },
    { "name": "Color Temp", "type": "macro", "default": 0, "ranges": [
      { "from": 0, "to": 9, "label": "Off" },
      { "from": 10, "to": 255, "label": "2500→6100K" }
    ]},
    { "name": "Segment", "type": "generic", "default": 0, "ranges": [
      { "from": 0, "to": 5, "label": "Off" },
      { "from": 6, "to": 65, "label": "Static 1–6" },
      { "from": 66, "to": 75, "label": "Off" },
      { "from": 76, "to": 255, "label": "Dynamic 7–18" }
    ]},
    { "name": "Segment Speed", "type": "speed", "default": 0 },
    { "name": "Zoom Auto", "type": "preset", "default": 0, "ranges": [
      { "from": 0, "to": 9, "label": "Off" },
      { "from": 10, "to": 255, "label": "Prog 1–8" }
    ]},
    { "name": "Auto/Sound", "type": "preset", "default": 0, "ranges": [
      { "from": 0, "to": 10, "label": "Off" },
      { "from": 11, "to": 90, "label": "P/T prog 1–8" },
      { "from": 91, "to": 230, "label": "Sound ctrl" },
      { "from": 231, "to": 255, "label": "Reset/Off" }
    ]}
  ]
}
```

Razlaga: Cold White dobi `"color_w"`, Warm White dobi `"color_a"` (ker sta dva bela kanala). Segment patterns ima 18 opcij → združi v 4 bloke.

---

### Primer 7: Stairville CX-30 RGB WW (8 kanalov, nenavaden vrstni red)

**Vhod:**
```
Ch1: Red, Ch2: Green, Ch3: Blue, Ch4: White (all if ch5=0…15 and ch7=0…31)
Ch5: Fixed colour — 0–15 off, 16–255 colors
Ch6: Strobe — 0–15 off, 16–255 speed
Ch7: Mode — 0–31 manual, 32–127 fade FX, 128–191 auto mix, 192–223 chase, 224–255 sound
Ch8: Dimmer 0–255
```

**Izhod** (`par-cx30-rgbww-8ch.json`):
```json
{
  "name": "CX-30 RGB WW 8ch",
  "channels": [
    { "name": "Red", "type": "color_r", "default": 0 },
    { "name": "Green", "type": "color_g", "default": 0 },
    { "name": "Blue", "type": "color_b", "default": 0 },
    { "name": "White", "type": "color_w", "default": 0 },
    { "name": "Fixed Color", "type": "macro", "default": 0, "ranges": [
      { "from": 0, "to": 15, "label": "Off (manual)" },
      { "from": 16, "to": 255, "label": "Colors 1–31" }
    ]},
    { "name": "Strobe", "type": "strobe", "default": 0, "ranges": [
      { "from": 0, "to": 15, "label": "Off" },
      { "from": 16, "to": 255, "label": "Slow → Fast" }
    ]},
    { "name": "Mode", "type": "preset", "default": 0, "ranges": [
      { "from": 0, "to": 31, "label": "Manual" },
      { "from": 32, "to": 127, "label": "Fade FX" },
      { "from": 128, "to": 223, "label": "Auto/Chase" },
      { "from": 224, "to": 255, "label": "Sound" }
    ]},
    { "name": "Dimmer", "type": "intensity", "default": 0 }
  ]
}
```

Razlaga: Vrstni red kanalov v profilu mora ustrezati DMX vrstnemu redu! Dimmer je na ch8 → zadnji v arrayu. Mode ima 7 sekcij → združi v 4.

---

## Zakaj je pravilno tipiziranje kanalov KRITIČNO

Naprava uporablja `type` polje za več ključnih funkcij:

1. **HSV → RGBW+A+UV pretvorba**: Barvno kolo v UI pošlje HSV na ESP32, ki **po tipu kanalov** razporedi barvo na R/G/B/W/A/UV kanale. Napačen tip = napačna barva!
2. **Sound-to-Light Easy Mode**: Bass poganja `intensity`, mid poganja `color_r`/`color_g`/`color_b`, high poganja `strobe`. Napačen tip = efekt ne deluje.
3. **Pixel Mapper**: Fixture Mirror način bere `color_r`/`color_g`/`color_b` za preslikavo na LED trak.
4. **Gang kontrola**: UI poveže kanale po tipu, ne po zaporedju. Napačen tip = gang ne deluje pravilno.
5. **Pametni Blackout**: Izklopi samo `intensity` in barvne kanale. `pan`/`tilt`/`gobo` ostanejo. Napačen tip = napačno vedenje.
6. **Locate/Highlight**: Nastavi `intensity`=255, barve=bela, `pan`/`tilt`=128, `zoom`=wide, `gobo`=0.

---

## Kontrolna lista pred oddajo

1. ☐ `name` profila ≤ 27 znakov
2. ☐ Število kanalov ≤ 16
3. ☐ **Vrstni red kanalov** ustreza DMX specifikaciji luči
4. ☐ Vsak `name` kanala ≤ 19 znakov
5. ☐ Vsak kanal ima max 4 ranges (ali pa ranges ni definiran)
6. ☐ Vsak range `label` ≤ 15 znakov
7. ☐ `type` je eden od dovoljenih nizov (glej tabelo zgoraj)
8. ☐ `default` je smiselna vrednost (0 za barve/dimmer, 128 za pan/tilt/focus)
9. ☐ Ranges pokrivajo celoten 0–255 brez praznin
10. ☐ JSON je veljaven (brez trailing vejic, pravilni oklepaji)
11. ☐ Linearni kanali (dimmer, barve) **NIMAJO** ranges
12. ☐ Ime datoteke: male črke, vezaji, `.json`, max 19+5 znakov
