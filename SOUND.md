# Sound-to-Light — Uporabniški priročnik

## 1. Kako deluje Sound-to-Light (STL)?

Naprava zajema zvok prek mikrofona (INMP441) ali line-in vhoda (WM8782S), ga analizira s FFT (Fast Fourier Transform) in razdeli v **8 frekvenčnih pasov**. Energija v vsakem pasu poganja svetlobne efekte na lučeh, ki imajo v patchu vklopljeno opcijo **"Zvok"** (Sound Reactive).

Signal potuje skozi verigo:

```
Mikrofon → FFT → AGC (ojačanje) → EQ (per-band gain) → Noise gate → Frekvenčni pasovi (Bass/Mid/High) → Mapiranje na DMX kanale
```

---

## 2. FFT spekter (vizualizacija)

Zgornja kartica prikazuje 8 stolpcev — po enega za vsak frekvenčni pas:

| Pas  | Obseg       | Opis                              |
|------|-------------|-----------------------------------|
| Sub  | 30–60 Hz    | Globoki bas (bas bobna, sub-bass) |
| Bass | 60–120 Hz   | Bas kitara, kick drum             |
| Low  | 120–250 Hz  | Spodnji mid, toplina vokala      |
| Mid  | 250–500 Hz  | Sredina — vokal, kitare           |
| Hi-M | 500–1000 Hz | Zgornji mid, jasnost             |
| Pres | 1–2 kHz     | Presence, razločnost             |
| Bril | 2–4 kHz     | Brilliance, činele               |
| Air  | 4–11 kHz    | Zrak, hi-hat, sibilanti          |

Merilci **Bass** / **Mid** / **High** pod spektrom prikazujejo povprečje spodnjih 3, srednjih 3 in zgornjih 2 pasov. **Peak** = trenutni vršni nivo vhoda, **BPM** = samodejno zaznani tempo, **Beat** = "!" ob zaznavi udarca.

---

## 3. Easy Mode

Najhitrejši način za zagon sound-to-light. Vklopi stikalo **"Vklopljeno"** in sistem samodejno mapira zvok na luči.

### Preseti

| Preset  | Opis                                | Najboljše za                        |
|---------|-------------------------------------|-------------------------------------|
| Pulse   | Samo bas → dimmer + beat bump       | Minimalistični efekt, eden LED par  |
| Rainbow | Mid → barvna rotacija, beat sync ON | Počasna atmosfera z barvami         |
| Storm   | Samo high → strobe + beat bump      | Agresiven strobe na činele          |
| Ambient | Bass + mid, nizka jakost (30%)      | Ozadje, lounge, restavracija        |
| Club    | Vse vklopljeno, 60%, beat sync ON   | Splošen klubski efekt               |
| Custom  | Ročna nastavitev vseh opcij         | Ko noben preset ne ustreza          |

### Parametri

- **Občutljivost** (0.1–5.0): Množilnik na vrhu AGC. Pri 1.0 izhod sledi vršni glasnosti. Pod 1.0 manj občutljivo, nad 1.0 bolj občutljivo.
- **Jakost** (0–100%): Koliko zvok prispeva k DMX izhodu. Pri 50% se zvočni efekt sešteje z ročnimi fader vrednostmi napol.
- **Bass → Dimmer**: Bass energija upravlja svetilnost (dimmer kanal). Glasnejši bas = svetlejše.
- **Mid → Barve**: Mid energija poganja barvno rotacijo (rainbow). Hue se vrti na vsakem fixture-u s 45° zamikom za raznolikost.
- **High → Strobe**: Visoke frekvence (činele, hi-hat) sprožijo strobe/shutter kanal.
- **Beat → Bump**: Ob zaznavi beata (bas udarec) se jakost poveča za 50% — ustvari pulz.
- **Beat Sync**: Efekti se sinhronizirajo z zaznano BPM. Brez: zvezna odzivnost. Z: pulziranje v ritmu.

### Primer — enostaven "club" efekt

1. V zavihku Fixtures označi luči kot "Zvok" (Sound Reactive)
2. Odpri Sound → Easy Mode → vklopi "Vklopljeno"
3. Izberi preset **Club**
4. Nastavi občutljivost na 1.0, jakost na 60%
5. Poženi glasbo — luči reagirajo na bas, barve se vrtijo, strobe utripa na činele

---

## 4. Audio EQ & AGC

**AGC (Automatic Gain Control)** samodejno prilagaja ojačanje glede na glasnost okolja. Brez AGC bi tiha glasba komaj ustvarila efekt, glasna pa bi vse saturirala.

### Okoljski preseti

| Preset     | Glasnost | AGC          | Gate        | Uporaba                                     |
|------------|----------|--------------|-------------|---------------------------------------------|
| Tiha soba  | ~50 dB   | Hiter (0.8)  | Nizek (0.1) | Testiranje doma, studijsko delo, govor       |
| Club / bar | ~80 dB   | Srednji (0.5)| Zmeren (0.3)| Manjši prostori z glasbo iz zvočnikov       |
| Koncert    | ~100 dB  | Počasen (0.2)| Visok (0.6) | Veliki odri, močen PA sistem                |
| Po meri    | —        | —            | —           | Ročna nastavitev vseh parametrov             |

### EQ drsniki (8 pasov)

Grafični izenačevalnik, ki množilnik (0.0–3.0) na vsakem frekvenčnem pasu. Privzeto 1.0 = brez spremembe.

- Povečaj **Sub + Bass** na 2.0 → močnejši odziv na bas boben
- Zmanjšaj **Air** na 0.3 → manj šumenja od visokih frekvenc
- Povečaj **Mid** na 1.5 → bolj izrazita barvna rotacija pri vokalu

### AGC hitrost (0–1.0)

Kako hitro se sistem prilagodi spremembi glasnosti.

- **Počasi (0.1–0.3)** = stabilno za šov, glasnost se ne spreminja hitro
- **Hitro (0.7–1.0)** = odzivno za testiranje pri različnih glasnostih

### Noise gate (0–1.0)

Filtrira šum ko ni glasbe.

- **0** = izključen (vedno reagira — tudi na šum klime)
- **0.3** = zmeren (ignorira tihe zvoke)
- **0.8+** = agresiven (samo glasna glasba gre skozi)

### Primer — nastavi za tiho sobo doma

1. Odpri Audio EQ & AGC → klikni **Tiha soba**
2. Sub in Bass se postavita na 2.0 (dvojno ojačanje za šibek bas iz telefona)
3. AGC hitrost: 0.8 (hitro prilagajanje)
4. Noise gate: 0.1 (nizek, da prepusti tudi tihe signale)
5. Poženi glasbo s telefona — luči morajo reagirati

---

## 5. Manual Beat

Ritmični efekti **brez zvočnega vira** — za situacije ko mikrofon ne deluje ali ko hočeš natančen tempo. Lahko deluje samostojno ali kot fallback za audio.

### Vir beata

- **Manual**: Tempo določiš s TAP gumbom ali BPM drsnikom. Vedno aktiven.
- **Auto (fallback)**: Ko je zvočni signal prisoten, se uporabi avdio beat detekcija. Ko signal izgine (tišina), se preklopi na manual tempo. *Priporočeno za žive nastope!*
- **Samo avdio**: Samo avdio beat detekcija. Ko ni signala, efekti utihnejo.

### TAP TEMPO

Pritisni gumb 4-krat v ritmu glasbe. Sistem izračuna BPM iz intervalov med pritiski. Lahko tudi ročno vpišeš BPM z drsnikom (30–300).

### Programi

| Program   | Opis                              | Efekt                                          |
|-----------|-----------------------------------|-------------------------------------------------|
| Pulse     | Intenziteta pada od beata         | Blisk na beat, nato postopno ugasne             |
| Chase     | Zaporedna aktivacija              | Ena luč naenkrat, po vrsti                      |
| Sine      | Gladka sinusoidna oscilacija      | Počasno dihanje svetlobe                        |
| Strobe    | Ostro vklop/izklop na beat        | Kratki blisk (15% faze) nato tema               |
| Rainbow   | Barvna rotacija sinhrona z beatom | Barve se vrtijo v tempu                         |
| Build     | Gradnja čez N beatov              | Intenziteta raste z vsakim beatom, nato reset   |
| Random    | Naključne barve ob beatu          | Vsaka luč dobi naključno barvo                  |
| Alternate | Sode/lihe izmenjujejo             | Luči 1,3,5 → beat 1; luči 2,4,6 → beat 2      |
| Wave      | Sinusni val s faznim zamikom      | Val svetlobe potuje čez luči                    |
| Stack     | Vsak beat doda luč                | Postopno prižiganje, nato reset                 |
| Sparkle   | Naključno utripanje               | Naključne luči kratko zasvetijo                 |
| Scanner   | Ena luč skenira levo-desno       | Kot "Knight Rider" — ping-pong                  |

### Subdivizija

Množilnik tempa:
- **1/4x** = 4x počasneje od BPM
- **1x** = normalen tempo
- **4x** = 4x hitreje (npr. 120 BPM → 480 udarcev/min)

### Dodatni parametri

- **Intenziteta** (0–100%): Maksimalna svetilnost efekta.
- **Barvni efekti**: Vklopi barvno spreminjanje (pri programih z barvami). Izklopi za čisto belo.
- **Build beatov** (4–32): Koliko beatov traja en cikel Build programa.
- **Krivulja zatemnitve**: Linear = enakomerno, Exponential = bolj dramatično, Logarithmic = mehkejše.
- **Attack** (0–500 ms): Čas od teme do polne svetilnosti. 0 = takojšen. 50+ ms = mehkejši vklop.
- **Decay** (0–2000 ms): Čas od polne svetilnosti do ugasnitve. 0 = privzeto (po programu). 500+ ms = dolg rep.
- **Paleta**: Izberi barvno shemo — Rainbow, Warm (rdeča/oranžna), Cool (modra/vijolična), Fire, Ocean, Party, ali Custom (4 barve po meri).

### Primer — chase efekt na 128 BPM

1. Vklopi Manual Beat
2. Vir: **Manual**
3. Nastavi BPM na 128 (drsnik ali 4x TAP)
4. Program: **Chase**
5. Subdivizija: **1x**
6. Intenziteta: 80%, Barvni efekti: ON, Paleta: Rainbow
7. Luči se po vrsti prižigajo v mavričnih barvah na vsak beat

### Primer — auto fallback za živi nastop

1. Nastavi Easy Mode z željenim presetom (npr. Club)
2. Vklopi Manual Beat z virom **Auto (fallback)**
3. Nastavi program Chase, BPM 120
4. Ko igra glasba → luči reagirajo na zvok (Easy Mode)
5. Ko nastane tišina (prehod med skladbami) → Manual Beat prevzame s Chaseom pri 120 BPM
6. Ko glasba spet zaživi → samodejno nazaj na audio

---

## 6. Program Chain (Playlist)

Avtomatsko zaporedje programov. Sistem samodejno preklopi program po zadanem številu beatov.

Primer: Pulse (8 beatov) → Chase (16 beatov) → Rainbow (8 beatov) → ponovi.

Vklopi **Chain**, dodaj vnose z gumbom "+", izberi program in trajanje za vsakega.

---

## 7. Cone (Per-fixture mapiranje)

Določi, kateri frekvenčni pas poganja posamezen fixture v Easy Mode:

- **Vse (All)**: Fixture reagira na celoten spekter (privzeto)
- **Bass**: Samo na bas
- **Mid**: Samo na srednje frekvence
- **High**: Samo na visoke frekvence

**Primer:** Postavi LED pare na **Bass**, moving heade na **Mid**, strobe na **High**. Rezultat: pari utripajo na bas boben, headi se vrtijo na vokal, strobe pa šika na činele.

---

## 8. Pro Mode — Pravila

Za napredne uporabnike. Ročno definiraj mapiranje: kateri frekvenčni obseg poganja kateri DMX kanal na katerem fixture-u.

Vsako pravilo ima:

- **Fixture + Kanal**: Kateri fixture in kateri kanal (dimmer, R, G, B...)
- **Freq**: Frekvenčni obseg (npr. 60–250 Hz za bas)
- **Izhod**: DMX razpon (npr. 0–255 za poln obseg, 50–200 za omejen)
- **Krivulja**: Linear, Exponential (bolj dramatično), Logarithmic (mehko), Square (koren)

Pro pravila delujejo neodvisno od Easy Mode — lahko imaš oboje hkrati.

---

## 9. LFO / FX Generator

Nizkofrekvenčni oscilator — ustvarja ponavljajoče vzorce na DMX kanalih **neodvisno od zvoka**.

- **Waveform**: Sine (gladko), Triangle (linearno), Square (ostro vklop/izklop), Saw (žagasto)
- **Target**: Na kateri parameter vpliva (Dimmer, Pan, Tilt, R, G, B)
- **Rate**: Hitrost oscilacije (Hz). 0.5 = en cikel na 2 sekundi, 2.0 = 2 cikla na sekundo.
- **Depth**: Amplituda (0–1). 0.5 = nihanje za +/-50% od središča.
- **Phase**: Fazni zamik med fixture-i (0–1). 0.25 = vsak fixture zamaknjen za četrtino cikla → val.

**Primer — dihanje svetlobe:** Sine, Dimmer, Rate 0.3, Depth 0.8, Phase 0. Na vseh fixture-ih se svetilnost počasi dviguje in spušča.

---

## 10. Shape Generator

Ustvarja gibalne vzorce za **Pan/Tilt** na moving head lučeh.

- **Circle**: Krožno gibanje
- **Figure-8**: Osmička
- **Triangle / Square**: Geometrične oblike
- **Line H / Line V**: Vodoravna ali navpična linija
- **Speed**: Hitrost gibanja
- **Size**: Velikost vzorca
- **Spread**: Fazni zamik med fixture-i

---

## 11. Master Speed

Globalni množilnik hitrosti za vse efekte (LFO, Shape, STL). **0.5x** = pol hitreje, **2.0x** = dvojna hitrost. Uporabno za hitro prilagoditev tempa vseh efektov hkrati.

---

## 12. Shranjevanje

Gumb **"Shrani nastavitve"** v vsaki kartici zapiše konfiguracijo v flash pomnilnik ESP32. Ob restartu se nastavitve naložijo. Brez shranjevanja se nastavitve ob izklopu izgubijo.

---

## Pogosti recepti

### Hitra demonstracija doma (telefon kot vir)

1. Fixtures → označi luči kot Sound Reactive
2. Sound → Audio EQ & AGC → **Tiha soba** preset
3. Sound → Easy Mode → **Club** preset
4. Predvajaj glasbo na telefonu blizu mikrofona
5. Luči reagirajo — prilagodi EQ po potrebi (povečaj Bass za več odziva na bas)

### Klubski nastop z DJ-em

1. Poveži line-in iz mešalne mize v WM8782S
2. Audio EQ & AGC → **Club / bar** preset
3. Easy Mode → **Club** preset, jakost 70%
4. Manual Beat → **Auto (fallback)**, program Chase, BPM 128
5. Ko DJ igra → avdio poganja efekte. V premorih → chase efekt vzdržuje vzdušje.

### Koncert — veliki oder

1. Line-in iz FOH mize
2. Audio EQ & AGC → **Koncert** preset
3. Zmanjšaj **Sub** na 0.5 (preveč sub-bassa iz PA sistema)
4. Easy Mode s presetom **Pulse** — samo bas poganja dimmer
5. Manual Beat → **Auto (fallback)**, program Strobe, za močne prehode

### Moving head show brez glasbe

1. Izklopi Easy Mode
2. Manual Beat → vir **Manual**, BPM 100
3. Program: **Chase**, subdivizija 1x
4. Shape Generator → Circle, Speed 0.5, Size 50%, Spread 0.25
5. Luči se vrtijo v krogu, prižigajo pa se zaporedno na beat

### Ambient osvetlitev za restavracijo

1. Audio EQ & AGC → **Tiha soba** preset
2. Easy Mode → **Ambient** preset (nizka jakost 30%)
3. Zmanjšaj High in Bril EQ na 0.3 (prepreči odziv na pogovor/krožnike)
4. Povečaj Mid na 1.5 → mehka barvna rotacija ob glasbi v ozadju

### Strobe na činele (EDM nastop)

1. Easy Mode → **Storm** preset
2. Audio EQ & AGC → povečaj **Bril** in **Air** na 2.0
3. Zmanjšaj **Sub** in **Bass** na 0.3 (ignoriraj bas — samo visoke frekvence)
4. Noise gate na 0.4 → strobe samo ko je glasba dovolj glasna

### Barvna sinhronizacija z beatom

1. Easy Mode → **Rainbow** preset
2. Manual Beat → vir **Auto (fallback)**, program Rainbow
3. Subdivizija: **1/2x** → barve se menjajo na vsak drugi beat
4. Paleta: **Warm** za toplejše barve ali **Ocean** za hladne tone
