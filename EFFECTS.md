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
