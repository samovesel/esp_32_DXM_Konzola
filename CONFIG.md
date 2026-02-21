# Fixtures, Nastavitve, Konfiguracija in DMX Monitor — Uporabniški priročnik

## FIXTURES (zavihek Fixtures)

### 1. Patchane luči

Tabela vseh dodanih fixture-ov s stolpci:

| Stolpec | Opis |
|---------|------|
| # | Zaporedna številka |
| Ime | Ime fixture-a |
| Profil | Ime profila (DMX modus) |
| DMX | DMX naslovni obseg (od–do) |
| Skupine | Članstvo v skupinah |
| Zvok | Zvočno reaktiven (Da/Ne) |

### 2. Dodajanje fixture-a

1. Klikni **"+ Dodaj luč"**
2. Izpolni polja:
   - **Ime** — opisno ime (npr. "PAR levo 1")
   - **DMX naslov** — samodejno predlagan naslednji prosti naslov
   - **Profil** — izberi iz naloženih profilov (prikazuje število kanalov)
   - **Skupine** — označi eno ali več skupin (1–8)
   - **Zvok** — omogoči zvočno reaktivnost za ta fixture
3. Klikni **"Dodaj"**

### 3. Urejanje fixture-a

Klikni **"Uredi"** ob fixture-u za spremembo:

- Ime, DMX naslov, skupine, zvočna reaktivnost
- **Pan/Tilt omejitve** (prikaže se samo za fixture-e s Pan/Tilt kanali):
  - **Obrni Pan / Obrni Tilt** — obrne os za zrcalno postavljene luči
  - **Pan min / max** (0–255) — omeji obseg gibanja Pan osi
  - **Tilt min / max** (0–255) — omeji obseg gibanja Tilt osi
  - Omejitve se aplicirajo na koncu render pipeline-a, tako da vse kontrole (faderji, LFO, shapes, sound) avtomatsko upoštevajo omejitve

### 4. Profili

Fixture profili so JSON datoteke, ki opisujejo DMX kanale naprave. Podpirajo več načinov (modes) za isti fixture.

- **Nalaganje**: Povleci JSON datoteko na območje za nalaganje ali klikni za izbiro
- **OFL uvoz**: Podprt je uvoz iz Open Fixture Library (OFL) — avtomatska pretvorba
- **Brisanje**: Klikni X ob profilu
- Podrobnosti o formatu: glej `PROFILE_SPEC.md`

### 5. Skupine

8 skupin za organizacijo fixture-ov:

- Vpiši ime za vsako skupino (npr. "Prednje", "Zadnje", "Moving Heads")
- Fixture-i se dodajo v skupine prek checkboxov ob dodajanju/urejanju
- Skupine omogočajo skupinsko kontrolo v Mixer zavihku in skupinske dimmerje

---

## NASTAVITVE (zavihek Nastavitve)

### 1. Naprava

- **Ime (hostname)** — mDNS ime za odkrivanje v omrežju (do 27 znakov)
- **ArtNet Univerza** — ArtNet universe index (0–32767)
- **Število DMX kanalov** — koliko kanalov se pošilja na DMX izhod (1–512)
- **ArtNet timeout** — sekunde brez ArtNet paketov pred preklopom na lokalno (1–3600, privzeto 10)
- **Primarni način** — ko je vklopljeno, ArtNet ne prevzame kontrole (lokalna kontrola ima prednost)
- **ArtNet izhod** — pošilja lokalni DMX state kot ArtNet pakete na omrežje
- **sACN (E1.31) izhod** — pošilja DMX prek sACN protokola

### 2. WiFi

Podpira do **5 WiFi omrežij** s samodejnim preklopom:

1. Klikni **"+ Dodaj omrežje"** ali **"Skeniraj WiFi"** za iskanje
2. Vpiši SSID in geslo
3. Prvo omrežje, ki se odzove, se uporabi
4. Če nobeno omrežje ni dosegljivo, se ESP32 postavi v AP način

**Statični IP:**
- Odklopi DHCP za ročno nastavljanje IP naslova, prehoda in maske

### 3. Avdio vhod

Izberi vir zvoka za Sound-to-Light:

| Možnost | Opis |
|---------|------|
| Izklopljeno | Brez avdio vhoda |
| Line-in I2S (WM8782S) | Zunanji line-in ADC, 96 kHz, profesionalna kvaliteta |
| I2S mikrofon (INMP441) | Vgrajeni MEMS mikrofon, za ambientno zaznavo |

**Opomba:** Sprememba avdio vira zahteva ponovni zagon.

### 4. Avtentikacija

- **Vklopi avtentikacijo** — zahteva prijavo za dostop do spletnega vmesnika
- Nastavi **uporabniško ime** in **geslo** (do 19 znakov)

### 5. Web MIDI

Podpora za MIDI kontrolerje prek Web MIDI API (deluje v Chrome/Edge):

- **Vklopi MIDI** — aktivira MIDI vhod
- **MIDI Learn** — klikni gumb, nato premakni fizični fader/gumb na kontrolerju za avtomatsko mapiranje
- **Mapiranja** — CC (Control Change) → faderji, Note → priklic scen
- **Zbriši mapiranja** — pobriši vse MIDI mapiranja

### 6. OSC Server

Vgrajen UDP Open Sound Control (OSC) strežnik na **portu 8000**.

Podprti naslovi:

| OSC naslov | Argumenti | Akcija |
|---|---|---|
| `/dmx/N` | float (0–1) | Nastavi DMX kanal N |
| `/fixture/N/dimmer` | float (0–1) | Nastavi dimmer fixture-a N |
| `/fixture/N/color` | float×3 | Nastavi R, G, B fixture-a N |
| `/scene/N` | — | Prikliči sceno N (s crossfade 1.5s) |
| `/master` | float (0–1) | Master dimmer |
| `/blackout` | int (0/1) | Blackout on/off |

Kompatibilen z: TouchOSC, QLab, Resolume, ipd.

### 7. Shranjevanje in ponastavitev

- **Shrani in ponovni zagon** — shrani vse nastavitve in resetira ESP32
- **Tovarniška ponastavitev** — pobriše vse nastavitve, fixture-e, scene in konfiguracijo. Zahteva potrditev

---

## KONFIGURACIJA (zavihek Konfig)

Shranjevanje in obnova celotne postavitve (vsi fixture-i, scene, skupine, zvočne nastavitve) kot snapshot.

### 1. Shranjevanje konfiguracije

1. Vpiši ime (npr. "Klub Petek", "Koncert Mali Oder")
2. Klikni **"Shrani trenutno"**
3. Konfiguracija se shrani v LittleFS flash pomnilnik

### 2. Nalaganje konfiguracije

Klikni **"Naloži"** ob shranjeni konfiguraciji — prepiše trenutno postavitev.

### 3. Uvoz / Izvoz

- **Prenesi** — prenesi konfiguracijo kot JSON datoteko na računalnik
- **Uvozi konfig iz datoteke** — naloži JSON datoteko z računalnika

### 4. Prostor na disku

Prikazuje porabo LittleFS flash pomnilnika. Opozorilo se prikaže, ko je prostora manj kot 20 KB.

---

## DMX MONITOR (zavihek DMX Mon)

Vizualni prikaz vseh 512 DMX kanalov v realnem času.

### Uporaba

1. Klikni **"Start Monitor"** za začetek spremljanja
2. Canvas prikazuje mrežo kanalov z barvnim kodiranjem vrednosti (temnejše = nižje, svetlejše = višje)
3. **Hover** nad kanalom prikaže številko kanala in trenutno vrednost
4. **Obseg** — izberi podmnožico kanalov za prikaz (1–512, 1–128, 129–256, 257–384, 385–512)

### Barvno kodiranje

- **Temno** (skoraj črno) = vrednost 0 (kanal neaktiven)
- **Svetlo zeleno/modro** = višje vrednosti
- **Polno** = vrednost 255 (kanal na maksimumu)

### Opomba

DMX Monitor pošilja podatke prek WebSocket-a in rahlo poveča obremenitev. Izklopi ga ko ni v uporabi.

---

## OTA posodobitev firmware-a

Gumb **⟲ FW** v zgornji vrstici odpre dialog za brezžično posodobitev:

1. Izberi `.bin` datoteko s firmware-om
2. Počakaj na nalaganje (ne izklopi naprave!)
3. Po uspešnem nalaganju se ESP32 samodejno resetira
