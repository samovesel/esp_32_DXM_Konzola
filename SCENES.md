# Scene in Cue List — Uporabniški priročnik

## 1. Crossfade

Drsnik na vrhu določa čas prehoda med scenami (0–10 sekund). Privzeto 1.5s.

- **0 ms** = takojšen preklop (brez prehoda)
- **10000 ms** = 10-sekundni počasni prehod
- Vrstica napredka prikazuje potek crossfade-a v realnem času
- Med crossfade-om kanali tipa Gobo/Prism/Shutter/Macro/Preset **preskočijo** na sredini (snap) namesto interpolacije — preprečuje grde vmesne pozicije

---

## 2. Scene

Do **20 scen** shranjenih v flash pomnilniku ESP32.

### Shranjevanje

1. Nastavi luči na želene vrednosti (ročno s faderji ali prek color pickerja)
2. Klikni **"Shrani trenutno stanje"**
3. Vpiši ime scene (npr. "Modra scena", "Refren")
4. Scena se shrani vključno z vsemi 512 DMX kanali

### Priklic (Recall)

Klikni gumb scene — sproži se crossfade iz trenutnega stanja v shranjeno sceno. Čas prehoda je določen z drsnikom Crossfade.

### Kontekstni meni (desni klik / dolg pritisk)

Desni klik na gumb scene (ali dolg pritisk na telefonu) odpre meni z možnostmi:

- **Preimenuj** — spremeni ime scene
- **Prepiši s trenutnim** — prepiše shranjeno sceno s trenutnim stanjem faderjev
- **Izbriši** — trajno izbriše sceno (zahteva potrditev)

### Predogled scene

Vsak gumb scene prikazuje majhne barvne pike, ki vizualno predstavljajo shranjene DMX vrednosti fixture-ov.

---

## 3. Cue List

Sekvenčno predvajanje scen — idealno za predstave, gledališke produkcije, live show-e.

### Kontrole

- **▶ GO** — poženi naslednjo sceno v seznamu (s crossfade-om)
- **◀ BACK** — vrni se na prejšnjo sceno
- **■ STOP** — ustavi predvajanje

### Dodajanje cue-jev

1. Izberi sceno iz spustnega menija
2. Nastavi **Fade** čas (ms) — crossfade za to specifično sceno
3. Nastavi **Auto** čas (ms) — samodejni prehod na naslednji cue po tem zamiku. 0 = ročni trigger z GO
4. Dodaj opcijsko **oznako** (do 23 znakov) — npr. "Začetek pesmi", "Solo"
5. Klikni **"+ Dodaj"**

### Primer cue lista

| # | Scena | Fade | Auto | Oznaka |
|---|-------|------|------|--------|
| 1 | Modra | 2.0s | 0 | Uvod |
| 2 | Rdeča | 1.0s | 5.0s | Refren |
| 3 | Bela | 0.5s | 0 | Solo |

V tem primeru: klikni GO za Uvod (modra, 2s prehod). Klikni GO za Refren (rdeča, 1s prehod). Po 5 sekundah se samodejno preklopi na Solo (bela, 0.5s prehod).

### Omejitve

- Do **40 cue-jev** v seznamu
- Cue list se shrani v `/cuelist.json` na flash pomnilniku
- Brisanje: klikni rdeči X ob vnosu

---

## 4. Scene v drugih pogledih

Scene so dostopne tudi iz:

- **Celozaslonska konzola** — scene gumbi na dnu zaslona
- **2D Oder** — scene gumbi na dnu zaslona
- **Shrani sceno** gumb je na voljo v obeh pogledih

---

## 5. Daljinsko upravljanje scen

Scene lahko priklicujete tudi prek zunanjih protokolov:

### OSC (Open Sound Control)

Pošljite OSC ukaz `/scene/N` (brez argumentov) na UDP port 8000 za priklic scene N (s crossfade-om 1.5s).

Kompatibilne aplikacije: **TouchOSC**, **QLab**, **Resolume**, ipd.

### Web MIDI

Z MIDI kontrolerjem (prek Chrome/Edge brskalnika):
- Uporabite **MIDI Learn** za mapiranje Note sporočil na scene
- Primer: Note C3 → Scene 1, Note D3 → Scene 2, itd.
- Podpira vse MIDI kontrolerje z Note izhodom

### Igralni plošček (Gamepad)

Z igralnim ploščkom (PlayStation/Xbox):
- **X gumb (□ na PS)** — Flash (prisilno 100% intenziteta na vseh fixture-ih, dokler držite)
- **B gumb (○ na PS)** — Blackout preklop

---

## 6. Multiplayer sinhronizacija

Ob sprožitvi scene se crossfade sinhronizira na **vse povezane naprave** prek WebSocket-a v realnem času:

- Ko operater sproži sceno na svoji napravi, se prehod prikaže na vseh ostalih zaslonih
- Več operaterjev lahko hkrati upravlja scene — zadnja sprožena scena prevlada
- Novo povezana naprava takoj prejme trenutno stanje (vključno z aktivnim crossfade-om)
- Primer: operater sproži Cue List s tabličnega računalnika, pomočnik na telefonu vidi napredek crossfade-a v realnem času
