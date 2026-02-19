# Specifikacija fixture profilov — ESP32 ArtNet/DMX Node

**Verzija:** 2.1
**Datum:** 2026-02-19
**Kompatibilnost:** Nazaj kompatibilno z v1.0 in v2.0 profili

---

## Pregled

Fixture profil je JSON datoteka, ki opisuje DMX kanale svetlobne naprave. Vsaka naprava ima lahko **več načinov** (modes), ki definirajo različne DMX konfiguracije.

Profili se naložijo na ESP32 prek web vmesnika (drag & drop) ali pa se obnovijo iz konfiguracijskega backup-a. Podprt je tudi **uvoz iz Open Fixture Library (OFL)** — OFL JSON se avtomatično pretvori v naš format ob nalogu.

---

## Omejitve

| Parameter             | Vrednost | Opomba                                           |
|----------------------|----------|--------------------------------------------------|
| Kanalov na način     | **24**   | Pokrije moving heade, segmentirane naprave        |
| Obsegov na kanal     | **6**    | Za kanale z več funkcijami — grupiraj če > 6   |
| Hkratno naloženih    | **16**   | Vsak fixture 1 aktiven mode; 14 naprav + 2 extra |
| Dolžina imena profila | 32 znakov | Ime naprave + način (npr. "Hero Wash 300 TW")   |
| Dolžina imena kanala  | 20 znakov | Kratko in jasno (npr. "CW Seg1")                |
| Dolžina labela obsega | 16 znakov | Kratek opis funkcije (npr. "Strobe Hz")          |
| Velikost datoteke     | < 4 KB   | Ena JSON datoteka za vse načine ene naprave       |

---

## Struktura JSON

```json
{
  "name": "Ime naprave",
  "comment": "Opcijski komentar (proizvajalec, model, opis)",
  "zoomRange": [4, 45],
  "modes": [
    {
      "name": "16ch",
      "channels": [
        {
          "name": "Dimmer",
          "type": "intensity",
          "default": 0,
          "ranges": [...]
        }
      ]
    }
  ]
}
```

### Polja na vrhnjem nivoju

| Polje     | Obvezno | Tip    | Opis                                           |
|-----------|---------|--------|-------------------------------------------------|
| `name`      | Da      | string | Ime naprave (max 32 znakov)                     |
| `comment`   | Ne      | string | Komentar za dokumentacijo (se ne prikaže v UI)  |
| `zoomRange` | Ne      | array  | Zoom kot v stopinjah [min, max], npr. [4, 45]. Uporablja se v 2D layout editorju za realistično vizualizacijo žarka. |
| `modes`     | Da      | array  | Seznam DMX načinov                              |

### Polja za način (mode)

| Polje      | Obvezno | Tip    | Opis                                           |
|------------|---------|--------|-------------------------------------------------|
| `name`     | Da      | string | Ime načina, prikazano v UI (npr. "16ch", "9ch") |
| `channels` | Da      | array  | Seznam kanalov (1-24 elementov)                 |

### Polja za kanal (channel)

| Polje     | Obvezno | Tip    | Opis                                            |
|-----------|---------|--------|-------------------------------------------------|
| `name`    | Da      | string | Ime kanala (max 20 znakov)                      |
| `type`    | Da      | string | Tip kanala — glej tabelo spodaj                 |
| `default` | Ne      | number | Privzeta vrednost 0-255 (default: 0)            |
| `ranges`  | Ne      | array  | Obsegi vrednosti s poimenovanimi funkcijami      |

### Polja za obseg (range)

| Polje   | Obvezno | Tip    | Opis                                            |
|---------|---------|--------|-------------------------------------------------|
| `range` | Da      | array  | Dvo-elementni array [od, do], vrednosti 0-255    |
| `label` | Da      | string | Kratek opis funkcije (max 16 znakov)             |

---

## Tipi kanalov

### Barvni kanali

| Tip          | Opis                    | Barva v UI     | Primer                    |
|-------------|-------------------------|----------------|---------------------------|
| `color_r`    | Rdeča (Red)            | rdeča          | `{ "name": "Red", "type": "color_r" }` |
| `color_g`    | Zelena (Green)         | zelena          | `{ "name": "Green", "type": "color_g" }` |
| `color_b`    | Modra (Blue)           | modra           | `{ "name": "Blue", "type": "color_b" }` |
| `color_w`    | Bela / hladna bela     | bela            | `{ "name": "Cold White", "type": "color_w" }` |
| `color_ww`   | Topla bela (Warm White)| topla bela      | `{ "name": "Warm White", "type": "color_ww" }` |
| `color_a`    | Amber                  | oranžna         | `{ "name": "Amber", "type": "color_a" }` |
| `color_uv`   | UV (Ultra Violet)      | vijolična       | `{ "name": "UV", "type": "color_uv" }` |
| `color_l`    | Lime                   | limeta          | `{ "name": "Lime", "type": "color_l" }` |
| `color_c`    | Cyan                   | cian            | `{ "name": "Cyan", "type": "color_c" }` |

### Intenziteta in nadzor

| Tip          | Opis                        | Primer                    |
|-------------|------------------------------|---------------------------|
| `intensity`  | Glavni dimmer (master)       | `{ "name": "Dimmer", "type": "intensity" }` |
| `strobe`     | Strobe / bliskavica          | Z `ranges` za različne efekte |
| `shutter`    | Zaslonka                     | Z `ranges` za open/closed |
| `cct`        | Barvna temperatura (CCT)     | `{ "name": "CCT", "type": "cct" }` |

### Gibanje

| Tip          | Opis                        | Primer                    |
|-------------|------------------------------|---------------------------|
| `pan`        | Pan (rotacija) — 8-bit      | Moving headi              |
| `pan_fine`   | Pan fine — 16-bit LSB       | Natančno pozicioniranje   |
| `tilt`       | Tilt (nagib) — 8-bit        | Moving headi              |
| `tilt_fine`  | Tilt fine — 16-bit LSB      | Natančno pozicioniranje   |
| `speed`      | Hitrost gibanja              | Pan/Tilt speed            |

### Optika in efekti

| Tip          | Opis                        | Primer                    |
|-------------|------------------------------|---------------------------|
| `zoom`       | Zoom (kot žarka)             | Wash / moving head        |
| `focus`      | Fokus                        | Spot moving head          |
| `gobo`       | Gobo kolo                    | Z `ranges` za vzorce      |
| `prism`      | Prizma                       | Efekt                     |
| `macro`      | Color/program macro          | Z `ranges` za funkcije    |
| `preset`     | Prednastavljeni programi     | Auto shows, sound active  |

### Splošno

| Tip          | Opis                        | Primer                    |
|-------------|------------------------------|---------------------------|
| `generic`    | Vse ostalo                   | Privzeto če tip ni znan   |

**Opomba o gang kontroli:** Ko krmilite več fixturov hkrati (gang), UI poveže kanale po **tipu**, ne po zaporedju. Zato je pravilno tipiziranje kanalov zelo pomembno — če ima en fixture `color_r` na kanalu 3 in drugi na kanalu 1, bo gang pravilno pošiljal rdečo na oba.

---

## Pravila za obsege (ranges)

1. **Največ 6 obsegov na kanal.** Če ima naprava več funkcij, jih grupirajte.
2. Obsegi morajo pokriti celoten razpon 0-255 brez vrzeli in prekrivanj.
3. Vsak obseg ima `[od, do]` kjer je `od <= do`.
4. Labeli morajo biti kratki in jasni (max 16 znakov).
5. Kanali brez obsegov (npr. `color_r` 0-255) ne potrebujejo `ranges` polja.

### Strategija grupiranja (ko ima naprava > 6 funkcij na kanalu)

Kadar ima naprava več kot 6 razdelkov na enem kanalu, jih združite v smiselne skupine:

**Primer: Segment pattern kanal z 18 vzorci**
```json
{
  "name": "Pattern",
  "type": "preset",
  "ranges": [
    { "range": [0, 5], "label": "Off" },
    { "range": [6, 65], "label": "Static 1-6" },
    { "range": [66, 75], "label": "No function" },
    { "range": [76, 255], "label": "Dynamic 7-18" }
  ]
}
```

**Primer: Strobe kanal z več funkcijami** (grupirano v 4)
```json
{
  "name": "Strobe",
  "type": "strobe",
  "ranges": [
    { "range": [0, 10], "label": "LEDs off" },
    { "range": [11, 140], "label": "Bright FX" },
    { "range": [141, 250], "label": "Pulse/Strobe" },
    { "range": [251, 255], "label": "LEDs on" }
  ]
}
```

---

## Polni primer: Moving Head z 19 kanali

```json
{
  "name": "Hero Wash 300 TW",
  "comment": "Varytec Hero Wash 300 TW — Tunable White, 3 segmenti",
  "modes": [
    {
      "name": "13ch",
      "channels": [
        { "name": "Pan", "type": "pan" },
        { "name": "Tilt", "type": "tilt" },
        { "name": "PT Speed", "type": "speed" },
        { "name": "Zoom", "type": "zoom" },
        { "name": "Dimmer", "type": "intensity" },
        {
          "name": "Strobe", "type": "strobe",
          "ranges": [
            { "range": [0, 10], "label": "Off" },
            { "range": [11, 200], "label": "Strobe FX" },
            { "range": [201, 250], "label": "Strobe Hz" },
            { "range": [251, 255], "label": "On" }
          ]
        },
        { "name": "Cold White", "type": "color_w" },
        { "name": "Warm White", "type": "color_ww" },
        {
          "name": "CCT", "type": "cct",
          "ranges": [
            { "range": [0, 9], "label": "Off" },
            { "range": [10, 255], "label": "2500K-6100K" }
          ]
        },
        {
          "name": "Pattern", "type": "preset",
          "ranges": [
            { "range": [0, 5], "label": "Off" },
            { "range": [6, 65], "label": "Static 1-6" },
            { "range": [66, 75], "label": "No function" },
            { "range": [76, 255], "label": "Dynamic 7-18" }
          ]
        },
        {
          "name": "Pat Speed", "type": "speed",
          "ranges": [
            { "range": [0, 127], "label": "Snap Speed" },
            { "range": [128, 255], "label": "Fade Speed" }
          ]
        },
        {
          "name": "Zoom FX", "type": "zoom",
          "ranges": [
            { "range": [0, 9], "label": "Off" },
            { "range": [10, 255], "label": "Auto Zoom" }
          ]
        },
        {
          "name": "Func", "type": "preset",
          "ranges": [
            { "range": [0, 10], "label": "No function" },
            { "range": [11, 90], "label": "Auto PT" },
            { "range": [91, 230], "label": "Sound Ctrl" },
            { "range": [231, 255], "label": "Reset" }
          ]
        }
      ]
    },
    {
      "name": "19ch",
      "channels": [
        { "name": "Pan", "type": "pan" },
        { "name": "Pan Fine", "type": "pan_fine" },
        { "name": "Tilt", "type": "tilt" },
        { "name": "Tilt Fine", "type": "tilt_fine" },
        { "name": "PT Speed", "type": "speed" },
        { "name": "Zoom", "type": "zoom" },
        { "name": "Dimmer", "type": "intensity" },
        {
          "name": "Strobe", "type": "strobe",
          "ranges": [
            { "range": [0, 10], "label": "LEDs off" },
            { "range": [11, 140], "label": "Bright FX" },
            { "range": [141, 250], "label": "Pulse/Strobe" },
            { "range": [251, 255], "label": "LEDs on" }
          ]
        },
        { "name": "CW Seg1", "type": "color_w" },
        { "name": "WW Seg1", "type": "color_ww" },
        { "name": "CW Seg2", "type": "color_w" },
        { "name": "WW Seg2", "type": "color_ww" },
        { "name": "CW Seg3", "type": "color_w" },
        { "name": "WW Seg3", "type": "color_ww" },
        {
          "name": "CCT", "type": "cct",
          "ranges": [
            { "range": [0, 9], "label": "No function" },
            { "range": [10, 255], "label": "2500K-6100K" }
          ]
        },
        {
          "name": "Pattern", "type": "preset",
          "ranges": [
            { "range": [0, 5], "label": "Off" },
            { "range": [6, 65], "label": "Static 1-6" },
            { "range": [66, 75], "label": "No function" },
            { "range": [76, 255], "label": "Dynamic 7-18" }
          ]
        },
        {
          "name": "Pat Speed", "type": "speed",
          "ranges": [
            { "range": [0, 127], "label": "Snap Speed" },
            { "range": [128, 255], "label": "Fade Speed" }
          ]
        },
        {
          "name": "Zoom FX", "type": "zoom",
          "ranges": [
            { "range": [0, 9], "label": "Off" },
            { "range": [10, 255], "label": "Auto Zoom" }
          ]
        },
        {
          "name": "Func", "type": "preset",
          "ranges": [
            { "range": [0, 10], "label": "No function" },
            { "range": [11, 90], "label": "Auto PT" },
            { "range": [91, 230], "label": "Sound Ctrl" },
            { "range": [231, 255], "label": "Reset" }
          ]
        }
      ]
    }
  ]
}
```

---

## Polni primer: RGBLAC PAR z dvema načinoma

```json
{
  "name": "2bright Par 720 IP",
  "comment": "Ignition 2bright Par 720 IP (RGBLAC)",
  "modes": [
    {
      "name": "9ch",
      "channels": [
        { "name": "Dimmer", "type": "intensity" },
        {
          "name": "Strobe", "type": "strobe",
          "ranges": [
            { "range": [0, 5], "label": "Open" },
            { "range": [6, 10], "label": "Closed" },
            { "range": [11, 127], "label": "Strobe FX" },
            { "range": [128, 255], "label": "Strobe/Open" }
          ]
        },
        { "name": "Red", "type": "color_r" },
        { "name": "Green", "type": "color_g" },
        { "name": "Blue", "type": "color_b" },
        { "name": "Lime", "type": "color_l" },
        { "name": "Amber", "type": "color_a" },
        { "name": "Cyan", "type": "color_c" },
        {
          "name": "Color Macro", "type": "macro",
          "ranges": [
            { "range": [0, 10], "label": "Off" },
            { "range": [11, 127], "label": "Fixed Colors" },
            { "range": [128, 191], "label": "Auto Jump" },
            { "range": [192, 255], "label": "Auto Fade" }
          ]
        }
      ]
    }
  ]
}
```

---

## Enostaven primer: RGBW PAR

```json
{
  "name": "Quad Par RGBW",
  "comment": "Stairville Quad Par 5x8W RGBW",
  "modes": [
    {
      "name": "4ch",
      "channels": [
        { "name": "Red", "type": "color_r" },
        { "name": "Green", "type": "color_g" },
        { "name": "Blue", "type": "color_b" },
        { "name": "White", "type": "color_w" }
      ]
    },
    {
      "name": "6ch",
      "channels": [
        { "name": "Red", "type": "color_r" },
        { "name": "Green", "type": "color_g" },
        { "name": "Blue", "type": "color_b" },
        { "name": "White", "type": "color_w" },
        { "name": "Dimmer", "type": "intensity" },
        {
          "name": "Strobe", "type": "strobe",
          "ranges": [
            { "range": [0, 10], "label": "Open" },
            { "range": [11, 255], "label": "Strobe S-F" }
          ]
        }
      ]
    }
  ]
}
```

---

## Spremembe glede na v2.0

| Sprememba                  | Staro (v2.0) | Novo (v2.1) |
|---------------------------|-------------|-------------|
| Max obsegov na kanal       | 4           | **6**       |
| Novo polje: `zoomRange`    | —           | Zoom kot [min, max] v stopinjah |
| OFL uvoz                   | —           | Avtomatska pretvorba OFL JSON   |

## Spremembe glede na v1.0

| Sprememba                  | Staro (v1.0) | Novo (v2.0) |
|---------------------------|-------------|-------------|
| Max kanalov na način       | 16          | **24**      |
| Max obsegov na kanal       | 4           | **4** (nespremenjeno)       |
| Max hkratnih profilov      | 24          | **16**      |
| Novi tipi: `color_ww`      | —           | Topla bela  |
| Novi tipi: `color_l`       | —           | Lime        |
| Novi tipi: `color_c`       | —           | Cyan        |
| Novi tipi: `cct`           | —           | Barvna temp.|

**Nazaj kompatibilnost:** Vsi obstoječi profili (v1.0 in v2.0) delujejo brez sprememb. Profili brez `zoomRange` uporabljajo privzete vrednosti (10-60°) v 2D layout editorju.
