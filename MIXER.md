# Mixer — Uporabniški priročnik

## 1. Zgornja vrstica (Header)

Zgornja vrstica je vidna na vseh zavihkih in vsebuje:

- **Status pika** — zelena = ArtNet aktiven, rumena = lokalna kontrola
- **Način (Mode)** — prikazuje trenutni način: ARTNET, LOKALNO (auto), LOKALNO (ročno), LOKALNO (prim.)
- **Preklopi način** — gumb za preklop med ArtNet in lokalno kontrolo
- **BLACKOUT** — takojšen izklop vseh svetlobnih kanalov (Intensity, barve, strobe). Pan/Tilt/Gobo/Focus/Zoom ostanejo nespremenjeni (pametni blackout)
- **FLASH** — drži gumb za prisilno 100% intenziteto na vseh fixture-ih. Deluje tudi med blackoutom. Spusti gumb za izklop
- **Undo** — razveljavi zadnjo spremembo faderjev (pokaže se šele ko je undo na voljo)

---

## 2. Master dimmer

Horizontalni drsnik (0–255) ki nadzoruje globalno svetilnost vseh fixture-ov. Vpliva samo na kanale tipa Intensity in barve (R/G/B/W/A/UV/L/C/WW). Pan/Tilt/Gobo niso prizadeti.

---

## 3. Pogledi

### Kompakt (Compact)

Privzeti pogled. Izberi enega ali več fixture-ov / skupin — prikaže se skupna kontrola s horizontalnimi drsniki.

- **Izbira fixture-a**: Klikni gumb z imenom fixture-a. Več izbranih = skupna kontrola vseh
- **Izbira skupine**: Gumbi skupin nad fixture gumbi. Izbrana skupina krmili vse fixture-e v njej
- **Kanali**: Horizontalni drsniki za vsak kanal izbranega fixture-a. Barva drsnika označuje tip kanala
- **Filmstrip gumbi**: Kanali z definiranimi obsegi (Gobo, Prism, Macro, Preset, Shutter) prikažejo mrežo gumbov namesto drsnika. Klikni gumb za izbiro funkcije
- **16-bit drsniki**: Pan in Tilt s fine kanalom prikažeta en drsnik z obsegom 0–65535 za natančno pozicioniranje

### Fan / Pahljača

Ko izberete **več fixture-ov hkrati**, se prikaže Fan vrstica:

- **Checkbox "Fan"** — vklopi/izklopi razpršitev vrednosti
- **Drsnik količine** — od -100% do +100%
  - 0% = vsi fixture-i dobijo isto vrednost
  - +100% = prvi fixture dobi najnižjo, zadnji najvišjo vrednost
  - -100% = obrnjeno (zadnji fixture dobi najnižjo)
- Uporabno za razpršitev Pan kota čez več moving headov (pahljača žarkov)

### Konzola (Console)

Vertikalni faderji za vse izbrane fixture-e — optimizirano za živo upravljanje.

- **Vertikalni drsniki** za vsak kanal vsakega fixture-a
- **Ime kanala** — klikni za skrčitev/razširitev posameznega kanala
- **Vrednost** — klikni za solo (skrije vse ostale kanale istega fixture-a)
- **Naslov fixture-a** — klikni za skritje/prikaz skrčenih kanalov
- **Drag & drop** — povleci fixture-e za prerazvrščanje vrstnega reda
- **Gang** — označi checkbox pod kanalom. Ko premakneš enega od povezanih kanalov, se vsi premaknejo za enako razliko
- **Dvojni klik na vrednost** — vnos natančnega številčnega podatka

### Celozaslonska konzola

Polni zaslon z vsemi funkcijami konzolnega pogleda, plus:

- Master dimmer v zgornji vrstici
- Crossfade vrstica na dnu
- Beat kontrole na dnu
- Scene gumbi na dnu (hitri recall)
- Skupinski dimmerji z gang podporo na dnu

### 2D Oder (Layout Editor)

Interaktivni vizualni oder za pozicioniranje luči v prostoru.

- **Oder**: Klikni "Oder" za vklop risanja odrskih elementov (pravokotniki). Povleci za risanje, klik ✕ za brisanje
- **Stranska vrstica (☰ Luči)**: Povleci fixture-e iz seznama na oder
- **Premikanje fixture-a**: Povleci fixture na odru za novo pozicijo. Povleci na smetnjak za odstranitev
- **Zoom**: Scroll kolesce ali pinch-to-zoom. Klik na indikator "100%" za reset
- **Klik na fixture** — odpre popup s:
  - Dimmer drsnik
  - Barvno kolo (color picker)
  - Pan/Tilt/Zoom drsniki (za moving heade)
  - Full On, Blackout, Locate gumbi
- **Zelena pika** — klikni za XY Pad popup (natančna Pan/Tilt kontrola z 2D vlečenjem)
- **Modra pika** — vleci ali scroll kolesce za Zoom kontrolo
- **Žarek (beam)** — vleci konico žarka za vizualno nastavljanje Pan/Tilt
- **Razporedi...** — samodejno razporedi fixture-e v vzorec:
  - Linija: vodoravna linija od leve do desne
  - Lok: polkrožni lok
  - Krog: enakomerna razporeditev v krogu
  - Mreža: pravokotna mreža (stolpci × vrstice)
- **Shrani layout** — shrani pozicije fixture-ov in odrske elemente. Možnih je več poimenovanih layoutov

---

## 4. Zgodovina stanj

Samodejni snapshoti ob preklopu med načini (ArtNet ↔ Lokalno). Klikni na vnos za obnovo stanja.

- **A** = stanje shranjeno iz ArtNet-a
- **L** = stanje shranjeno iz lokalne kontrole
- **Obnovi ArtNet senco** — naloži zadnje ArtNet stanje

---

## 5. Barvna paleta (Color Picker)

V kompakt pogledu ali 2D layout popupu klikni na barvni kanal fixture-a za odprtje barvnega kola:

- **Barvno kolo** — klikni/vleci za izbiro barve (Hue + Saturation)
- **Dimmer drsnik** — nastavi svetilnost
- **Predogled** — kvadrat z izbrano barvo + HEX koda
- **Paleta hitrih barv** — vnaprej definirane barve za hitro izbiro

---

## 6. XY Pad popup

Za natančno Pan/Tilt kontrolo na moving headih:

- **Canvas 200×200px** — horizontalna os = Pan, vertikalna os = Tilt
- **Klikni/vleci** — nastavi Pan in Tilt hkrati
- Prikazuje trenutne vrednosti (0–255) za oba kanala
