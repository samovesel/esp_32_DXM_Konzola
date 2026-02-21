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

### 6. Igralni plošček (Gamepad)

Podpora za igralne ploščke prek brskalnikovega Gamepad API-ja (deluje v Chrome/Edge).

#### Omogočanje

- **Vklopi igralni plošček** — aktivira Gamepad vhod v spletnem vmesniku

#### Združljivi kontrolerji

- PlayStation DualShock (PS3, PS4) in DualSense (PS5)
- Xbox kontrolerji (Xbox One, Xbox Series)
- Generični USB ali Bluetooth igralni ploščki

#### Mapiranje kontrol

| Kontrola | Funkcija |
|----------|----------|
| Levi analog (stick) | Pan/Tilt za Skupino 1 |
| Desni analog (stick) | Pan/Tilt za Skupino 2 |
| R2 sprožilec (trigger) | Master dimmer (0–255) |
| Gumb X (□ na PlayStation) | Flash — drži za polno intenziteto |
| Gumb B (○ na PlayStation) | Blackout preklop (toggle) |

#### Mrtva cona (Deadzone)

- Nastavljiva mrtva cona za analogne palice (privzeto **15 %**)
- Preprečuje nehoteno premikanje (drift), ko palica ni dotaknjena
- Nižja vrednost = večja občutljivost, višja = manj neželenih premikov

#### Primer uporabe

1. Poveži PS5 kontroler prek Bluetooth-a na prenosni računalnik
2. Odpri spletni vmesnik konzole v brskalniku Chrome
3. V zavihku Nastavitve vklopi **Igralni plošček**
4. Kontroliraj Pan/Tilt s palicama, Master dimmer z R2, Blackout z gumbom ○

### 7. Pixel Mapper — LED trak (samo ESP32-S3)

Krmiljenje zunanjih WS2812 / NeoPixel LED trakov prek RMT periferije (brez obremenitve procesorja).

#### Priključek

- **Pin:** GPIO 16 (fiksno določen)

#### Nastavitve

- **Vklopi / Izklopi** — omogoči ali onemogoči LED trak
- **Število LED-ic** — nastavi od 1 do 144
- **Svetlost** — nastavi od 0 do 255
- **Način delovanja (Mode):**

| Način | Opis |
|-------|------|
| **Fixture Mirror** | Preslika RGB barve izbranega fixture-a ali skupine na LED segmente. Izberi fixture ali skupino. |
| **VU Meter** | Indikator glasnosti zvoka: zelena (nizko) → rumena (srednje) → rdeča (visoko) |
| **Spectrum** | 8 FFT frekvenčnih pasov prikazanih kot mavrično obarvane stolpce vzdolž traku |
| **Beat Pulse** | Vse LED-ice utripnejo na udarec (beat) z vrteče se spreminjajočo barvo (hue) |

- **Izbira fixture-a / skupine** — vidno samo v načinu Fixture Mirror; izberi kateri fixture ali skupino se preslika na trak

#### Shranjevanje

Konfiguracija Pixel Mapper-ja se shrani v datoteko `/pixmap.bin` in se ohrani ob ponovnem zagonu.

#### Primer uporabe

1. Namesti 60-LED WS2812 trak za DJ pult
2. Poveži podatkovni pin traku na GPIO 16 na ESP32-S3
3. V nastavitvah vklopi Pixel Mapper, nastavi 60 LED-ic
4. Izberi način **Spectrum** — uživaj v vizualizaciji zvoka v realnem času

### 8. ESP-NOW — Brezžični DMX

Brezžični prenos DMX signala prek ESP-NOW protokola brez potrebe po usmerjevalniku (routerju).

#### Omogočanje

- **Vklopi / Izklopi** — omogoči ali onemogoči ESP-NOW oddajanje

#### Upravljanje sprejemnikov (slave)

- **Dodaj sprejemnik** — vpiši MAC naslov v formatu `AA:BB:CC:DD:EE:FF`
- Podprtih je do **4 sprejemnikov** (peer-ov)
- **Odstrani** — klikni gumb X ob sprejemniku za odstranitev
- **Shrani** — konfiguracija se ohrani ob ponovnem zagonu

#### Kako deluje

1. **Master ESP32** (konzola) razdeli 512-bajtni DMX buffer na pakete po 200 bajtov
2. Paketi se pošljejo prek **ESP-NOW** peer-to-peer protokola z latenco **< 1 ms**
3. **Slave ESP32** (cenovna rešitev: ESP32 modul + MAX485, ~3 EUR) prejme pakete in sestavi DMX buffer
4. Slave pošlje DMX signal prek UART na MAX485 → XLR izhod

#### Opomba

ESP-NOW deluje na istem Wi-Fi kanalu kot AP način konzole — ni potreben zunanji usmerjevalnik. Povezava je direktna med napravami.

#### Primer uporabe

1. Ena master konzola za glavni oder
2. Dva slave sprejemnika — eden na stranskem odru, eden na balkonu
3. Brezžični DMX povsod, brez kablov in brez routerja

### 9. OSC Server

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

### 10. Shranjevanje in ponastavitev

- **Shrani in ponovni zagon** — shrani vse nastavitve in resetira ESP32
- **Tovarniška ponastavitev** — pobriše vse nastavitve, fixture-e, scene in konfiguracijo. Zahteva potrditev

---

## SOUND (zavihek Sound)

Zavihek Sound vsebuje vse nastavitve za zvočno reaktivno razsvetljavo (Sound-to-Light).

### 1. Easy Mode

Hitra nastavitev zvočne reaktivnosti s preseti:

- **Vklopljeno** — omogoči Easy Mode
- **Preseti**: Custom, Pulse, Rainbow, Storm, Ambient, Club
- **Občutljivost** (0.1–5.0) — kako močno reagirajo luči na zvok
- **Jakost** (0–100%) — maksimalna svetilnost efekta
- **Bass → Dimmer** — bas frekvence krmilijo intenziteto
- **Mid → Barve (Rainbow)** — srednje frekvence spreminjajo barve
- **High → Strobe** — visoke frekvence sprožijo stroboskop
- **Beat → Bump** — beat detekcija sproži blisk
- **Beat Sync** — efekti se sinhronizirajo z BPM

### 2. Audio EQ & AGC

Parametrični izenačevalnik in avtomatsko prilagajanje ojačanja.

#### Okolje (preseti)

| Preset | Opis |
|--------|------|
| Tiha soba | Visoko ojačanje, nizek noise gate. Za šibke signale (telefon, tiha glasba). |
| Club / bar | Srednje ojačanje, zmeren noise gate. Privzeto za manjše prostore. |
| Koncert | Nizko ojačanje, visok noise gate. Za glasna okolja z močnim basom. |
| Po meri | Ročno nastavljanje vseh parametrov. |

#### Parametric EQ

8 pasov z nastavljivimi parametri:

| Parameter | Obseg | Opis |
|-----------|-------|------|
| **Gain** | 0.0–3.0 | Ojačanje pasu (navpični drsnik) |
| **Center freq** | 20–11000 Hz | Središčna frekvenca pasu (vnosno polje) |
| **Q faktor** | Dropdown (0.3–5.0) | Širina pasu — nižje = širše, višje = ožje |

Q faktor prednastavljene vrednosti:

| Q | Opis |
|-----|------|
| 0.3 | Zelo širok (~3 oktave) |
| 0.5 | Širok (~2 oktavi) |
| 0.7 | Privzeto (~1.5 oktave) |
| 1.0 | Srednji (~1 oktava) |
| 1.4 | Ozek (~0.7 oktave) |
| 2.0 | Tesen (~0.5 oktave) |
| 3.0 | Oster (~0.33 oktave) |
| 5.0 | Kirurški (~0.2 oktave) |

#### AGC

- **AGC hitrost** (0.0–1.0) — počasi = stabilno za koncerte (~10s prilagajanje), hitro = odzivno za testiranje (~0.7s)
- **Noise gate** (0.0–1.0) — 0 = izključen, višje = agresivnejše filtriranje šuma v tišini

#### Beat detekcija

Neodvisna od parametričnega EQ — bere surove FFT bine za zanesljivejšo zaznavo.

| Parameter | Obseg | Privzeto | Opis |
|-----------|-------|----------|------|
| **Občutljivost** | 0.8x–2.5x | 1.4x | Prag glede na drseče povprečje. Nižje = bolj občutljivo. |
| **Frekv. pas** | Dropdown | Bass (30–150 Hz) | Frekvenčno območje za zaznavo beata |
| **Lockout** | 100–500 ms | 200 ms | Min razmik med zaznavo dveh beatov |

Frekvenčni preseti:

| Preset | Obseg | Uporaba |
|--------|-------|---------|
| Sub | 30–80 Hz | Samo globok bas (sub-woofer) |
| Bass | 30–150 Hz | Privzeto — kick drum + bas |
| Wide | 30–300 Hz | Širši spekter za raznoliko glasbo |
| Kick | 60–200 Hz | Optimirano za kick drum |

BPM se izračuna z **median filtrom** iz zadnjih 16 inter-beat intervalov za stabilnost.

### 3. Manual Beat

Ritmični efekti z nastavljivi virom beata.

#### Viri beata

| Vir | Opis |
|-----|------|
| **Manual** | Tempo določiš s TAP gumbom ali BPM drsnikom (30–300). Vedno aktiven. |
| **Avdio BPM** | Programi tečejo, BPM se samodejno sinhronizira iz avdio FFT analize. |
| **Auto (fallback)** | Zvok prisoten → avdio beat. Tišina → manual tempo. Priporočeno za žive nastope. |
| **Samo avdio** | Samo avdio beat detekcija. Ko ni signala, efekti utihnejo. |
| **Ableton Link** | BPM in faza se sinhronizirata prek Ableton Link protokola z DJ programsko opremo. |

#### Programi

12 programov: Pulse, Chase, Sine, Strobe, Rainbow, Build, Random, Alternate, Wave, Stack, Sparkle, Scanner.

#### Parametri

- **BPM** (30–300) — ročno ali samodejno (odvisno od vira)
- **Subdivizija** — 1/4x, 1/2x, 1x, 2x, 4x
- **Jakost** (0–100%)
- **Barvni efekti** — vklop/izklop barvnega spreminjanja
- **Build beatov** (4–32) — trajanje Build cikla
- **Krivulja** — Linear, Exponential, Logarithmic
- **Attack** (0–500 ms) — čas od teme do polne svetilnosti
- **Decay** (0–2000 ms) — čas od polne svetilnosti do ugasnitve
- **Paleta** — Rainbow, Warm, Cool, Fire, Ocean, Party, Custom (4 barve)
- **Simetrija** — Forward, Reverse, Center-Out, Ends-In

#### Program Chain (Playlist)

Avtomatsko zaporedje programov. Vsak vnos ima program in trajanje v beatih.

### 4. Cone (per-fixture mapiranje)

Določi, kateri frekvenčni pas poganja posamezen fixture v Easy Mode:

| Cona | Opis |
|------|------|
| Bass | Reagira na nizke frekvence |
| Mid | Reagira na srednje frekvence |
| High | Reagira na visoke frekvence |
| All | Reagira na celoten spekter |

### 5. Pro Mode — Pravila

Do 8 pravil za natančno kontrolo posameznih kanalov na osnovi frekvenčnih pasov. Vsako pravilo določa fixture, kanal, frekvenčni pas in krivuljo odziva.

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

Gumb **⟲ FW** v zgornji vrstici odpre dialog za brezžično posodobitev (Over-The-Air):

1. Izberi `.bin` datoteko s firmware-om
2. Počakaj na nalaganje (ne izklopi naprave!)
3. Po uspešnem nalaganju se ESP32 samodejno resetira

Posodobitve firmware-a je mogoče izvesti brezžično kadarkoli prek spletnega vmesnika — fizični dostop do naprave ni potreben. Na ta način lahko namestite nove funkcionalnosti, popravke napak in izboljšave brez potrebe po USB povezavi.
