# Sound-to-Light — Uporabniski prirocnik

> **Novo v tej verziji:** FFT analiza je sedaj strojno pospeena (hardware-accelerated) z uporabo knjiznice **ESP-DSP**. Na ESP32-S3 se uporablja Vector ISA instrukcijski nabor, kar je priblizno **3x hitrejse** od prejsnje ArduinoFFT implementacije. To omogoca nizjo latenco in boljso odzivnost pri vseh Sound-to-Light efektih.

> **Persona vmesniki:** Sound-to-Light je dostopen tudi prek poenostavljenih persona vmesnikov — **Tonski mojster** (`/sound-eng`) ima STL toggle s preseti za okolje, **DJ** (`/dj`) vkljucuje beat programe in BPM sync, **Busker** (`/busker`) ima STL stikalo za avtomatsko zvocno reaktivnost. Konfiguracija presetov: zavihek Konfig > Persona vmesniki.

## 1. Kako deluje Sound-to-Light (STL)?

Naprava zajema zvok prek mikrofona (INMP441) ali line-in vhoda (WM8782S), ga analizira s FFT (Fast Fourier Transform) in razdeli v **8 frekvencnih pasov**. Energija v vsakem pasu poganja svetlobne efekte na luceh, ki imajo v patchu vklopljeno opcijo **"Zvok"** (Sound Reactive).

Signal potuje skozi verigo:

```
Mikrofon → I2S → ESP-DSP FFT (hardware-accelerated) → AGC → Parametric EQ → Noise gate → Frekvencni pasovi → DMX
```

ESP-DSP FFT izkoristi strojne zmoznosti ESP32 cipaː
- Na **ESP32-S3** se uporablja Vector ISA (SIMD instrukcije) za paralelno obdelavo FFT
- Na **ESP32 (klasicen)** se ESP-DSP prevede v optimizirano C kodo, ki je se vedno hitrejsa od genericne ArduinoFFT
- FFT tabele se alocirajo v PSRAM, ce je na voljo

---

## 2. FFT spekter (vizualizacija)

Zgornja kartica prikazuje 8 stolpcev — po enega za vsak frekvencni pas. Privzete center frekvence (nastavljive prek parametric EQ):

| Pas  | Privzeto (center) | Q privzeto | Opis                              |
|------|--------------------|------------|-----------------------------------|
| 1    | 42 Hz              | 0.7        | Globoki bas (bas bobna, sub-bass) |
| 2    | 85 Hz              | 0.7        | Bas kitara, kick drum             |
| 3    | 173 Hz             | 0.7        | Spodnji mid, toplina vokala      |
| 4    | 354 Hz             | 0.7        | Sredina — vokal, kitare           |
| 5    | 707 Hz             | 0.7        | Zgornji mid, jasnost             |
| 6    | 1414 Hz            | 0.7        | Presence, razlocnost             |
| 7    | 2828 Hz            | 0.7        | Brilliance, cinele               |
| 8    | 6633 Hz            | 0.7        | Zrak, hi-hat, sibilanti          |

Center frekvenca in Q faktor vsakega pasu sta nastavljiva prek **parametric EQ** (glej poglavje 4). Privzete vrednosti ustrezajo klasicni razdelitvi Sub/Bass/Low/Mid/Hi-M/Pres/Bril/Air.

Merilci **Bass** / **Mid** / **High** pod spektrom prikazujejo povprecje spodnjih 3, srednjih 3 in zgornjih 2 pasov. **Peak** = trenutni vrsni nivo vhoda, **BPM** = samodejno zaznani tempo, **Beat** = "!" ob zaznavi udarca.

---

## 3. Easy Mode

Najhitrejsi nacin za zagon sound-to-light. Vklopi stikalo **"Vklopljeno"** in sistem samodejno mapira zvok na luci.

### Preseti

| Preset  | Opis                                | Najboljse za                        |
|---------|-------------------------------------|-------------------------------------|
| Pulse   | Samo bas → dimmer + beat bump       | Minimalisticni efekt, eden LED par  |
| Rainbow | Mid → barvna rotacija, beat sync ON | Pocasna atmosfera z barvami         |
| Storm   | Samo high → strobe + beat bump      | Agresiven strobe na cinele          |
| Ambient | Bass + mid, nizka jakost (30%)      | Ozadje, lounge, restavracija        |
| Club    | Vse vklopljeno, 60%, beat sync ON   | Splosen klubski efekt               |
| Custom  | Rocna nastavitev vseh opcij         | Ko noben preset ne ustreza          |

### Parametri

- **Obcutljivost** (0.1–5.0): Mnozilnik na vrhu AGC. Pri 1.0 izhod sledi vrsni glasnosti. Pod 1.0 manj obcutljivo, nad 1.0 bolj obcutljivo.
- **Jakost** (0–100%): Koliko zvok prispeva k DMX izhodu. Pri 50% se zvocni efekt sesteje z rocnimi fader vrednostmi napol.
- **Bass → Dimmer**: Bass energija upravlja svetilnost (dimmer kanal). Glasnejsi bas = svetlejse.
- **Mid → Barve**: Mid energija poganja barvno rotacijo (rainbow). Hue se vrti na vsakem fixture-u s 45 stopinjskim zamikom za raznolikost.
- **High → Strobe**: Visoke frekvence (cinele, hi-hat) sprozijo strobe/shutter kanal.
- **Beat → Bump**: Ob zaznavi beata (bas udarec) se jakost poveca za 50% — ustvari pulz.
- **Beat Sync**: Efekti se sinhronizirajo z zaznano BPM. Brez: zvezna odzivnost. Z: pulziranje v ritmu.

### Primer — enostaven "club" efekt

1. V zavihku Fixtures oznaci luci kot "Zvok" (Sound Reactive)
2. Odpri Sound → Easy Mode → vklopi "Vklopljeno"
3. Izberi preset **Club**
4. Nastavi obcutljivost na 1.0, jakost na 60%
5. Pozeni glasbo — luci reagirajo na bas, barve se vrtijo, strobe utripa na cinele

---

## 4. Audio Parametric EQ & AGC

**AGC (Automatic Gain Control)** samodejno prilagaja ojacanje glede na glasnost okolja. Brez AGC bi tiha glasba komaj ustvarila efekt, glasna pa bi vse saturirala.

### Okoljski preseti

| Preset     | Glasnost | AGC          | Gate        | Uporaba                                     |
|------------|----------|--------------|-------------|---------------------------------------------|
| Tiha soba  | ~50 dB   | Hiter (0.8)  | Nizek (0.1) | Testiranje doma, studijsko delo, govor       |
| Club / bar | ~80 dB   | Srednji (0.5)| Zmeren (0.3)| Manjsi prostori z glasbo iz zvocnikov       |
| Koncert    | ~100 dB  | Pocasen (0.2)| Visok (0.6) | Veliki odri, mocen PA sistem                |
| Po meri    | —        | —            | —           | Rocna nastavitev vseh parametrov             |

### Parametric EQ (8 pasov)

Polno parametricni izenacevalnik z nastavljivimi parametri za vsak pas:

- **Gain** (0.0–3.0): Mnozilnik energije pasu. Privzeto 1.0 = brez spremembe.
- **Center frekvenca** (20–11000 Hz): Srediscna frekvenca zvonaste krivulje. Nastavi na tocno frekvenco, ki jo zelis izpostaviti (npr. 80 Hz za kick drum, 400 Hz za vokal).
- **Q faktor**: Sirina pasu. Izbiraj med prednastavljenimi vrednostmi:

| Q   | Sirina     | Uporaba                              |
|-----|------------|--------------------------------------|
| 0.3 | Zelo sirok | ~3 oktave, zajame celoten razpon     |
| 0.5 | Sirok      | ~2 oktavi, splosen poudarek          |
| 0.7 | Privzeto   | ~1.5 oktave, uravnotezeno            |
| 1.0 | Srednji    | ~1 oktava, zmeren poudarek           |
| 1.4 | Ozek       | ~0.7 oktave, bolj natancno           |
| 2.0 | Tesen      | ~0.5 oktave, precizno ciljanje       |
| 3.0 | Oster      | ~0.33 oktave, izolacija frekvence    |
| 5.0 | Kirurski   | ~0.2 oktave, ozek notch/boost        |

Primeri uporabe:

- Povecaj **gain** na bandu 1 (85 Hz) na 2.0 → mocnejsi odziv na bas boben
- Zmanjsaj **gain** na bandu 8 (6.6 kHz) na 0.3 → manj sumenja od visokih frekvenc
- Premakni **center frekvenco** banda 4 na 400 Hz in povecaj **Q** na 3.0 → ozek pas za vokal
- Premakni **center frekvenco** banda 1 na 60 Hz in **Q** na 5.0 → precizen odziv na kick drum brez basa kitare

> **Opomba:** Beat detekcija od verzije V6 dalje deluje neodvisno od parametric EQ — bere neposredno iz raw FFT binov v nastavljenem frekvencnem obmocju (privzeto 30–150 Hz). Spreminjanje parametric EQ ne vpliva na beat detekcijo.

### AGC hitrost (0–1.0)

Kako hitro se sistem prilagodi spremembi glasnosti.

- **Pocasi (0.1–0.3)** = stabilno za sov, glasnost se ne spreminja hitro
- **Hitro (0.7–1.0)** = odzivno za testiranje pri razlicnih glasnostih

### Noise gate (0–1.0)

Filtrira sum ko ni glasbe.

- **0** = izkljucen (vedno reagira — tudi na sum klime)
- **0.3** = zmeren (ignorira tihe zvoke)
- **0.8+** = agresiven (samo glasna glasba gre skozi)

### Primer — nastavi za tiho sobo doma

1. Odpri Audio EQ & AGC → klikni **Tiha soba**
2. Gain na prvih dveh bandih se postavi na 2.0 (dvojno ojacanje za sibek bas iz telefona)
3. AGC hitrost: 0.8 (hitro prilagajanje)
4. Noise gate: 0.1 (nizek, da prepusti tudi tihe signale)
5. Pozeni glasbo s telefona — luci morajo reagirati

### Primer — ozek pas za kick drum

1. Odpri Audio EQ & AGC → klikni **Po meri**
2. Band 1: nastavi center frekvenco na **60 Hz**, Q na **5.0**, gain na **2.0**
3. Band 2: nastavi center frekvenco na **120 Hz**, Q na **3.0**, gain na **1.5**
4. Ostale bande pusti na privzetih vrednostih
5. Vizualni odziv bo zdaj natancno sledil kick drumu

### Beat detekcija

Beat detekcija deluje **neodvisno od parametric EQ** — bere neposredno iz raw FFT binov v nastavljenem frekvencnem obmocju. Premikanje EQ bandov za vizualne efekte ne vpliva na zaznavo beatov.

- **Obcutljivost** (0.8x–2.5x): Prag za zaznavo beata. Nizje = bolj obcutljivo (sproze na sibke beate). Visje = samo mocni udari. Privzeto 1.4x.
- **Frekv. pas**: Frekvencno obmocje za beat detekcijo:
  - **Sub (30–80 Hz)** — samo globok bas, brez kitare
  - **Bass (30–150 Hz)** — privzeto, pokriva kick drum in bas
  - **Wide (30–300 Hz)** — sirok pas za raznovrsten ritem
  - **Kick (60–200 Hz)** — ozek pas za kick drum
- **Lockout** (100–500 ms): Minimalni razmik med dvema beatoma. Nizje = dovoli hitrejse beate (EDM z double-kick). Visje = stabilnejse za pocasno glasbo. Privzeto 200 ms.

BPM se izracuna z **median filtrom** (namesto povprecja) — to pomeni, da posamezen zgresen ali podvojen beat ne premakne BPM izracuna. Sistem hrani zadnjih 16 inter-beat intervalov in vzame mediano, kar zagotavlja stabilen BPM prikaz.

### Primer — nastavitev za EDM

1. Odpri Audio EQ & AGC → Beat detekcija
2. Obcutljivost: **1.2x** (nizka, za mocne kicke v EDM)
3. Frekv. pas: **Kick (60–200 Hz)**
4. Lockout: **150 ms** (dovoli do ~400 BPM za double-kick)
5. BPM se stabilno prikazuje v statusni vrstici

---

## 5. Manual Beat

Ritmicni efekti **brez zvocnega vira** — za situacije ko mikrofon ne deluje ali ko hoces natancen tempo. Lahko deluje samostojno ali kot fallback za audio.

### Vir beata

- **Manual**: Tempo dolocis s TAP gumbom ali BPM drsnikom. Vedno aktiven.
- **Avdio BPM**: Manualni programi (Pulse, Chase, Strobe…) tecejo, ampak BPM se samodejno sinhronizira iz avdio analize (FFT beat detekcija). BPM se glajeno prilagaja (eksponentno glajenje, cas prilagajanja ~2s), faza se resetira ob zaznavi beata za tesen lock. BPM drsnik je onemogocen v tem nacinu. *Idealno ko hoces manual beat programe, ampak ne zelis rocno nastavljati BPM.*
- **Auto (fallback)**: Ko je zvocni signal prisoten, se uporabi avdio beat detekcija. Ko signal izgine (tisina), se preklopi na manual tempo. *Priporoceno za zive nastope!*
- **Samo avdio**: Samo avdio beat detekcija. Ko ni signala, efekti utihnejo.
- **Ableton Link**: BPM in beat faza se sinhronizirata prek Wi-Fi z DJ software-om (Ableton Live, Traktor, Rekordbox, Serato, VirtualDJ). Zahteva, da sta ESP32 in racunalnik/telefon z DJ appom na istem Wi-Fi omrezju. Vec o tem v poglavju [6. Ableton Link](#6-ableton-link).

### TAP TEMPO

Pritisni gumb 4-krat v ritmu glasbe. Sistem izracuna BPM iz intervalov med pritiski. Lahko tudi rocno vpises BPM z drsnikom (30–300).

### Programi

| Program   | Opis                              | Efekt                                          |
|-----------|-----------------------------------|-------------------------------------------------|
| Pulse     | Intenziteta pada od beata         | Blisk na beat, nato postopno ugasne             |
| Chase     | Zaporedna aktivacija              | Ena luc naenkrat, po vrsti                      |
| Sine      | Gladka sinusoidna oscilacija      | Pocasno dihanje svetlobe                        |
| Strobe    | Ostro vklop/izklop na beat        | Kratki blisk (15% faze) nato tema               |
| Rainbow   | Barvna rotacija sinhrona z beatom | Barve se vrtijo v tempu                         |
| Build     | Gradnja cez N beatov              | Intenziteta raste z vsakim beatom, nato reset   |
| Random    | Nakljucne barve ob beatu          | Vsaka luc dobi nakljucno barvo                  |
| Alternate | Sode/lihe izmenjujejo             | Luci 1,3,5 → beat 1; luci 2,4,6 → beat 2      |
| Wave      | Sinusni val s faznim zamikom      | Val svetlobe potuje cez luci                    |
| Stack     | Vsak beat doda luc                | Postopno priziganje, nato reset                 |
| Sparkle   | Nakljucno utripanje               | Nakljucne luci kratko zasvetijo                 |
| Scanner   | Ena luc skenira levo-desno       | Kot "Knight Rider" — ping-pong                  |

### Subdivizija

Mnozilnik tempa:
- **1/4x** = 4x pocasneje od BPM
- **1x** = normalen tempo
- **4x** = 4x hitreje (npr. 120 BPM → 480 udarcev/min)

### Dodatni parametri

- **Intenziteta** (0–100%): Maksimalna svetilnost efekta.
- **Barvni efekti**: Vklopi barvno spreminjanje (pri programih z barvami). Izklopi za cisto belo.
- **Build beatov** (4–32): Koliko beatov traja en cikel Build programa.
- **Krivulja zatemnitve**: Linear = enakomerno, Exponential = bolj dramaticno, Logarithmic = mehkejse.
- **Attack** (0–500 ms): Cas od teme do polne svetilnosti. 0 = takojsen. 50+ ms = mehkejsi vklop.
- **Decay** (0–2000 ms): Cas od polne svetilnosti do ugasnitve. 0 = privzeto (po programu). 500+ ms = dolg rep.
- **Paleta**: Izberi barvno shemo — Rainbow, Warm (rdeca/oranzna), Cool (modra/vijolicna), Fire, Ocean, Party, ali Custom (4 barve po meri).

### FX Simetrija (Manual Beat)

Gumbi pod beat programi dolocajo vrstni red fixture-ov za efekte:

- **→ Naprej** — normalno (1→2→3→4)
- **← Nazaj** — obratno (4→3→2→1)
- **↔ Sredina ven** — iz sredine navzven
- **⇄ Zunaj not** — od zunaj proti sredini

Uporabno za Chase, Wave, Stack in Scanner programe — npr. simetrija "Sredina ven" ustvari efekt ki se siri od centra odra.

### Primer — chase efekt na 128 BPM

1. Vklopi Manual Beat
2. Vir: **Manual**
3. Nastavi BPM na 128 (drsnik ali 4x TAP)
4. Program: **Chase**
5. Subdivizija: **1x**
6. Intenziteta: 80%, Barvni efekti: ON, Paleta: Rainbow
7. Luci se po vrsti prizigajo v mavricnih barvah na vsak beat

### Primer — auto fallback za zivi nastop

1. Nastavi Easy Mode z zeljenim presetom (npr. Club)
2. Vklopi Manual Beat z virom **Auto (fallback)**
3. Nastavi program Chase, BPM 120
4. Ko igra glasba → luci reagirajo na zvok (Easy Mode)
5. Ko nastane tisina (prehod med skladbami) → Manual Beat prevzame s Chaseom pri 120 BPM
6. Ko glasba spet zazivi → samodejno nazaj na audio

### Primer — avdio BPM sinhronizacija

1. Vklopi Manual Beat
2. Vir: **Avdio BPM**
3. Program: **Rainbow**, Subdivizija: **1x**
4. Pozeni glasbo — sistem samodejno zazna BPM iz glasbe
5. Rainbow efekt se vrti v tempu glasbe, BPM se prikazuje v statusni vrstici
6. Ce zamenjsa skladbo z drugim tempom, se BPM glajeno prilagodi v ~2s
7. BPM drsnik je onemogocen — tempo prihaja iz avdia

---

## 6. Ableton Link

### Kaj je Ableton Link?

Ableton Link je **Wi-Fi UDP multicast protokol** za sinhronizacijo beat grida med napravami. Razvil ga je Ableton, a ga podpira sirok nabor DJ in glasbene programske opreme. Protokol omogoca, da vec naprav na istem Wi-Fi omrezju deli skupen BPM in beat fazo — brez centralnega streznika, brez kablov, brez MIDI.

ESP32 se prek Link protokola prikljuci v isto beat mrezo kot DJ software, kar pomeni, da se vsi svetlobni efekti (Pulse, Chase, Rainbow, Strobe...) natancno sinhronizirata s tempom, ki ga doloca DJ.

### Podprta programska oprema

| Software          | Platforma           | Opombe                                 |
|-------------------|---------------------|----------------------------------------|
| Ableton Live      | macOS, Windows      | Izvorna podpora (razvili Link)         |
| Traktor Pro       | macOS, Windows      | Gumb "Link" v nastavitvah              |
| Rekordbox         | macOS, Windows      | Podpora od verzije 6.0                 |
| Serato DJ         | macOS, Windows      | Gumb "Link" v zgornjem meniju          |
| VirtualDJ         | macOS, Windows      | Podpora v Professional verziji         |
| djay (Algoriddim) | macOS, iOS, Android | Mobilna DJ aplikacija s Link podporo   |
| Reason            | macOS, Windows      | DAW z Link podporo                     |

Poleg DJ software podpira Link tudi mnogo mobilnih aplikacij (drum machine, synth, looper) — vsaka naprava, ki podpira Ableton Link, se samodejno sinhronizirata z ESP32.

### Kako uporabiti Ableton Link

**Korak 1: Povezi ESP32 in DJ racunalnik na isto Wi-Fi omrezje**

ESP32 in racunalnik (ali telefon) z DJ software-om morata biti na istem Wi-Fi omrezju. Link uporablja UDP multicast, zato mora Wi-Fi router dovoliti multicast promet (vecina domacih routerjev to podpira privzeto).

**Korak 2: Vklopi Link v DJ software-u**

- **Traktor Pro**: Preferences → Link → Enable
- **Ableton Live**: Gumb "Link" v zgornjem levem kotu
- **Rekordbox**: Settings → Audio → Ableton Link → ON
- **Serato DJ**: Setup → DJ Preferences → Enable Ableton Link
- **VirtualDJ**: Settings → Options → Ableton Link

**Korak 3: Izberi Link kot vir beata v ESP32 spletnem vmesniku**

1. Odpri spletni vmesnik ESP32 (obicajno http://192.168.x.x)
2. Pojdi na **Sound → Manual Beat**
3. Pri **Vir beata** izberi **"Ableton Link"**
4. Sistem se samodejno poveze z Link sejo na Wi-Fi omrezju

**Korak 4: Preveri status povezave**

V statusni vrstici se prikazejo:
- **Peers**: Stevilo povezanih naprav (npr. "2 peers" = ESP32 + DJ software)
- **BPM**: Sinhroniziran tempo (npr. "128.0 BPM" — prevzet iz DJ software-a)
- **Phase**: Trenutna beat faza (0.0–1.0) — prikazana kot utripajoc indikator

Ce je stevilo peers 0, pomeni da DJ software se ni vklopil Linka ali da naprave niso na istem omrezju.

**Korak 5: Uzivaj v sinhroniziranih efektih**

Vsi beat programi (Pulse, Chase, Rainbow, Strobe, Build, Alternate, Wave, Stack, Sparkle, Scanner) se sedaj sinhronizirajo z Link beat gridom. Ko DJ spremeni BPM, se ESP32 samodejno prilagodi. Beat faza je natancno poravnana — luci utripnejo tocno na beat, ne z zaostankom.

### LED statusni prikaz

Na fizicni napravi ESP32 LED indikator prikazuje stanje Link povezave:
- **Ugasnjeno**: Link ni aktiven
- **Pocasno utripanje**: Link aktiven, iscem vrstnike (0 peers)
- **Hitro utripanje na beat**: Link povezan in sinhroniziran

### Zahteve za namestitev

Ableton Link funkcionalnost zahteva namestitev ustrezne knjiznice:

- **Knjiznica**: Ableton Link ESP32 port
- **GitHub**: https://github.com/pschatzmann/esp32-esp-idf-abl-link
- **Namestitev**: Kopiraj mapo `include/ableton` v `Arduino/libraries/AbletonLink/`
- **Preverjanje**: Ce je knjiznica pravilno nalozena, se v serijskem monitorju ob zagonu izpise: `[LINK] Ableton Link knjiznica najdena`
- **Brez knjiznice**: Ce knjiznica NI nalozena, modul deluje kot **stub** — opcija "Ableton Link" je vidna v meniju, a ob izbiri se izpise opozorilo in funkcionalnost ne deluje. Vse ostale funkcije (Manual, Auto fallback, Samo avdio) delujejo normalno.

### Primeri uporabe z Ableton Link

#### DJ set s Traktor Pro

1. Povezi line-in iz DJ mize v WM8782S na ESP32
2. V Traktor Pro: Preferences → Link → Enable
3. V ESP32 spletnem vmesniku: Sound → Manual Beat → Vir beata → **Ableton Link**
4. Preveri status: "2 peers", BPM prikazan (npr. 126 BPM)
5. Vklopi Easy Mode → **Club** preset
6. Nastavi Manual Beat program na **Chase** za prehode med skladbami
7. Luci samodejno sledijo BPM iz Traktor-ja — ko DJ spremeni tempo, se luci prilagodijo

#### Multi-ESP32 sinhronizacija

Vec ESP32 kontrolerjev na istem Wi-Fi omrezju se lahko vsi sinhronizirata prek Ableton Link:

1. Povezi vse ESP32 naprave na isto Wi-Fi omrezje
2. Na vsaki ESP32: Sound → Manual Beat → Vir beata → **Ableton Link**
3. Aktiviraj Link v DJ software-u (ali v katerikoli aplikaciji, ki podpira Link)
4. Vse ESP32 naprave delijo isti BPM in beat fazo
5. Rezultat: luci po celem prostoru (npr. oder, bar, plesisce) utripajo v popolni sinhronizaciji

Ce nimas DJ software-a, lahko uporabis katerokoli brezplacno mobilno aplikacijo z Ableton Link podporo (npr. "Link to MIDI" na iOS) kot "master clock" — telefon doloca tempo, vsi ESP32-ji pa sledijo.

#### Ableton Live produkcija s svetlobnim sovom

1. V Ableton Live: vklopi Link (zgornji levi kot)
2. ESP32 na istem Wi-Fi: Vir beata → **Ableton Link**
3. Program: **Build** z 16 beati — svetloba raste skozi 4 takte
4. Ko v Abletonu sprozis drop, se svetloba prav tako "eksplodira" na pravem mestu
5. Subdivizija **2x** za hitrejsi odziv na hi-hat vzorce

---

## 7. Program Chain (Playlist)

Avtomatsko zaporedje programov. Sistem samodejno preklopi program po zadanem stevilu beatov.

Primer: Pulse (8 beatov) → Chase (16 beatov) → Rainbow (8 beatov) → ponovi.

Vklopi **Chain**, dodaj vnose z gumbom "+", izberi program in trajanje za vsakega.

Program Chain deluje z vsemi viri beata, vkljucno z Ableton Link — preklop programa se zgodi na pravilnem mestu v beat gridu.

---

## 8. Cone (Per-fixture mapiranje)

Doloci, kateri frekvencni pas poganja posamezen fixture v Easy Mode:

- **Vse (All)**: Fixture reagira na celoten spekter (privzeto)
- **Bass**: Samo na bas
- **Mid**: Samo na srednje frekvence
- **High**: Samo na visoke frekvence

**Primer:** Postavi LED pare na **Bass**, moving heade na **Mid**, strobe na **High**. Rezultat: pari utripajo na bas boben, headi se vrtijo na vokal, strobe pa sika na cinele.

---

## 9. HSV Barvno kolo (Mixer)

### Kako deluje

Barvni izbiralnik (color picker) v spletnem vmesniku sedaj poslje **HSV vrednosti** (Hue, Saturation, Value) neposredno na ESP32, namesto posameznih RGB kanalov. ESP32 nato izvede pretvorbo v ustrezne DMX kanale glede na profil fixture-a.

### Signalna pot

```
Color Picker (HSV) → WebSocket → ESP32 → HSV→RGB→RGBW+A+UV → DMX kanali
```

### Prednosti

- **Manj WebSocket prometa**: En sam HSV paket namesto 6–8 posameznih kanalskih sporocil (R, G, B, W, A, UV). To zmanjsa obremenitev Wi-Fi povezave in pohitri odzivnost.
- **Profil-zavedna pretvorba**: ESP32 samodejno pretvori barvo v ustrezne kanale glede na profil fixture-a:
  - **RGB fixture**: HSV → R, G, B
  - **RGBW fixture**: HSV → R, G, B + izracun belega kanala (W) iz desaturiranega dela barve
  - **RGBWA fixture**: HSV → R, G, B, W + Amber izracunan iz toplih tonov
  - **RGBWAUV fixture**: HSV → R, G, B, W, A + UV izracunan iz hladnih/vijolicnih tonov
- **Konsistentnost**: Ista HSV barva izgleda enako na vseh tipih fixture-ov, ker ESP32 optimalno razporedi energijo med kanali.

### Uporaba

1. V zavihku **Mixer** klikni na barvno kolo ob zelenemu fixture-u
2. Izberi barvo z vrtenjem po krogu (Hue) in premikanjem po trikotniku (Saturation + Value)
3. Barva se takoj poslje na ESP32 kot HSV paket
4. ESP32 pretvori v RGB/RGBW/RGBWAUV glede na profil fixture-a
5. Slider za "V" (Value/svetilnost) pod kolesom omogoca neodvisno nastavitev svetilnosti brez spreminjanja barve

### Tehnincni detajl

Ce ima fixture profil z W (beli), A (amber) ali UV kanaloma, ESP32 izracuna optimalno razporeditev:
- **Beli kanal (W)**: Ko je nasicenost (S) nizka, se del energije prenese na beli kanal za cistozso belo svetlobo
- **Amber kanal (A)**: Ko je Hue v toplem obmocju (15–60 stopinj), se del energije prenese na amber za bogatejse tople tone
- **UV kanal**: Ko je Hue v vijolicnem obmocju (270–330 stopinj), se del energije prenese na UV za globljo vijolicno

---

## 10. Pro Mode — Pravila

Za napredne uporabnike. Rocno definiraj mapiranje: kateri frekvencni obseg poganja kateri DMX kanal na katerem fixture-u.

Vsako pravilo ima:

- **Fixture + Kanal**: Kateri fixture in kateri kanal (dimmer, R, G, B...)
- **Freq**: Frekvencni obseg (npr. 60–250 Hz za bas)
- **Izhod**: DMX razpon (npr. 0–255 za poln obseg, 50–200 za omejen)
- **Krivulja**: Linear, Exponential (bolj dramaticno), Logarithmic (mehko), Square (koren)

Pro pravila delujejo neodvisno od Easy Mode — lahko imas oboje hkrati.

---

## 11. LFO / FX Generator

Nizkofrekventni oscilator — ustvarja ponavljajoce vzorce na DMX kanalih **neodvisno od zvoka**.

- **Waveform**: Sine (gladko), Triangle (linearno), Square (ostro vklop/izklop), Saw (zagasto)
- **Target**: Na kateri parameter vpliva (Dimmer, Pan, Tilt, R, G, B)
- **Rate**: Hitrost oscilacije (Hz). 0.5 = en cikel na 2 sekundi, 2.0 = 2 cikla na sekundo.
- **Depth**: Amplituda (0–1). 0.5 = nihanje za +/-50% od sredisca.
- **Phase**: Fazni zamik med fixture-i (0–1). 0.25 = vsak fixture zamaknjen za cetrtino cikla → val.
- **Symmetry**: Simetrija faznega razporeda:
  - **→ Naprej** — normalno (1→2→3→4)
  - **← Nazaj** — obratno (4→3→2→1)
  - **↔ Sredina** — iz sredine navzven
  - **⇄ Zunaj** — od zunaj proti sredini

**Primer — dihanje svetlobe:** Sine, Dimmer, Rate 0.3, Depth 0.8, Phase 0. Na vseh fixture-ih se svetilnost pocasi dviguje in spusca.

---

## 12. Shape Generator

Ustvarja gibalne vzorce za **Pan/Tilt** na moving head luceh.

- **Circle**: Krozno gibanje
- **Figure-8**: Osmicka
- **Triangle / Square**: Geometricne oblike
- **Line H / Line V**: Vodoravna ali navpicna linija
- **Speed**: Hitrost gibanja
- **Size**: Velikost vzorca
- **Spread**: Fazni zamik med fixture-i

---

## 13. Master Speed

Globalni mnozilnik hitrosti za vse efekte (LFO, Shape, STL). **0.5x** = pol hitreje, **2.0x** = dvojna hitrost. Uporabno za hitro prilagoditev tempa vseh efektov hkrati.

---

## 14. Shranjevanje

Gumb **"Shrani nastavitve"** v vsaki kartici zapise konfiguracijo v flash pomnilnik ESP32. Ob restartu se nastavitve nalozijo. Brez shranjevanja se nastavitve ob izklopu izgubijo.

To velja tudi za nastavitve Ableton Link — ce je Link izbran kot vir beata in nastavitve shranjene, se ob ponovnem zagonu ESP32 samodejno poskusi povezati z Link sejo na Wi-Fi.

---

## Pogosti recepti

### Hitra demonstracija doma (telefon kot vir)

1. Fixtures → oznaci luci kot Sound Reactive
2. Sound → Audio EQ & AGC → **Tiha soba** preset
3. Sound → Easy Mode → **Club** preset
4. Predvajaj glasbo na telefonu blizu mikrofona
5. Luci reagirajo — prilagodi EQ po potrebi (povecaj Bass za vec odziva na bas)

### Klubski nastop z DJ-em

1. Povezi line-in iz mesalne mize v WM8782S
2. Audio EQ & AGC → **Club / bar** preset
3. Easy Mode → **Club** preset, jakost 70%
4. Manual Beat → **Auto (fallback)**, program Chase, BPM 128
5. Ko DJ igra → avdio poganja efekte. V premorih → chase efekt vzdrzuje vzdusje.

### Koncert — veliki oder

1. Line-in iz FOH mize
2. Audio EQ & AGC → **Koncert** preset
3. Zmanjsaj **Sub** na 0.5 (prevec sub-bassa iz PA sistema)
4. Easy Mode s presetom **Pulse** — samo bas poganja dimmer
5. Manual Beat → **Auto (fallback)**, program Strobe, za mocne prehode

### Moving head show brez glasbe

1. Izklopi Easy Mode
2. Manual Beat → vir **Manual**, BPM 100
3. Program: **Chase**, subdivizija 1x
4. Shape Generator → Circle, Speed 0.5, Size 50%, Spread 0.25
5. Luci se vrtijo v krogu, prizigajo pa se zaporedno na beat

### Ambient osvetlitev za restavracijo

1. Audio EQ & AGC → **Tiha soba** preset
2. Easy Mode → **Ambient** preset (nizka jakost 30%)
3. Zmanjsaj High in Bril EQ na 0.3 (prepreci odziv na pogovor/kroznike)
4. Povecaj Mid na 1.5 → mehka barvna rotacija ob glasbi v ozadju

### Strobe na cinele (EDM nastop)

1. Easy Mode → **Storm** preset
2. Audio EQ & AGC → povecaj **Bril** in **Air** na 2.0
3. Zmanjsaj **Sub** in **Bass** na 0.3 (ignoriraj bas — samo visoke frekvence)
4. Noise gate na 0.4 → strobe samo ko je glasba dovolj glasna

### Barvna sinhronizacija z beatom

1. Easy Mode → **Rainbow** preset
2. Manual Beat → vir **Auto (fallback)**, program Rainbow
3. Subdivizija: **1/2x** → barve se menjajo na vsak drugi beat
4. Paleta: **Warm** za toplejse barve ali **Ocean** za hladne tone

### DJ set z Ableton Link (Traktor)

1. Povezi line-in iz DJ mize v WM8782S
2. Audio EQ & AGC → **Club / bar** preset
3. V Traktor Pro: Preferences → Link → Enable
4. V ESP32 spletnem vmesniku: Sound → Manual Beat → Vir beata → **Ableton Link**
5. Preveri: "2 peers" in BPM se ujema s Traktor-jem
6. Easy Mode → **Club** preset, jakost 70%
7. Manual Beat program: **Chase** — luci sledijo beat gridu iz Traktor-ja
8. Ko DJ preide na novo skladbo in spremeni BPM, se luci samodejno prilagodijo

### Multi-ESP32 sinhroniziran oder

1. Postavi vec ESP32 kontrolerjev po prostoru (oder, bar, plesisce, vhod)
2. Vse povezi na isto Wi-Fi omrezje
3. Na vsaki ESP32: Vir beata → **Ableton Link**
4. DJ vklopi Link v svojem software-u
5. Vse luci po prostoru utripajo v popolni sinhronizaciji z DJ-em
6. Vsaka ESP32 ima lahko drug program (npr. oder = Chase, bar = Pulse, plesisce = Rainbow)
7. Skupen BPM in beat faza zagotavljata, da je celoten prostor v ritmu

### Ableton Live produkcija + svetlobni sov

1. V Ableton Live: vklopi Link
2. ESP32 na istem Wi-Fi: Vir beata → **Ableton Link**
3. Nastavi Easy Mode → **Club** preset
4. Manual Beat program → **Build** z 16 beati
5. V Abletonu pripravljen arrangement z build-up in drop sekcijami
6. Svetloba samodejno gradi intenziteto cez 4 takte in "eksplodira" na drop
7. Za se vec dinamike: Program Chain → Build (16b) → Strobe (4b) → Chase (8b) → ponovi

### Brezplacni Link master clock (brez DJ software-a)

1. Ce nimas DJ software-a, nalozi brezplacno "Link to MIDI" app na telefon (iOS/Android)
2. V aplikaciji nastavi zelen BPM (npr. 125) in vklopi Link
3. Na ESP32: Vir beata → **Ableton Link**
4. Telefon deluje kot master clock — ESP32 sledi tempu
5. BPM lahko spreminjas v realnem casu na telefonu, luci se takoj prilagodijo
