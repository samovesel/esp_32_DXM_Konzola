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

## 3. Igralni plošček (Gamepad)

Mixer podpira povezavo igralnega ploščka (gamepad) za hitro in intuitivno upravljanje luči v živo.

### Kontrole

- **Leva palica** — krmili Pan/Tilt za vse fixture-e v **Skupini 1** (Group 1). Levo/desno = Pan, gor/dol = Tilt
- **Desna palica** — krmili Pan/Tilt za vse fixture-e v **Skupini 2** (Group 2). Levo/desno = Pan, gor/dol = Tilt
- **R2 sprožilec (trigger)** — nastavlja Master dimmer proporcionalno (analogno). Brez pritiska = 0%, poln pritisk = 255
- **Gumb X** — Flash (drži za prisilno polno intenziteto na vseh fixture-ih). Spusti za vrnitev v prejšnje stanje
- **Gumb B** — Blackout preklop (toggle). En klik vklopi blackout, naslednji ga izklopi

### Primer uporabe

DJ z igralnim ploščkom: z levo palico usmeri sprednje wash luči po odru, z desno palico krmili moving heade za odrom, z R2 sprožilcem nastavlja skupno svetilnost. Gumb X za flash ob vrhuncih glasbe, gumb B za takojšen blackout pred naslednjo pesmijo.

> **Opomba:** Gamepad mora biti podprt s strani brskalnika (Gamepad API). Večina modernih brskalnikov podpira standardne USB in Bluetooth igralne ploščke. Ob priključitvi se gamepad samodejno zazna.

---

## 4. Pogledi

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

## 5. Zgodovina stanj

Samodejni snapshoti ob preklopu med načini (ArtNet ↔ Lokalno). Klikni na vnos za obnovo stanja.

- **A** = stanje shranjeno iz ArtNet-a
- **L** = stanje shranjeno iz lokalne kontrole
- **Obnovi ArtNet senco** — naloži zadnje ArtNet stanje

---

## 6. Barvna paleta (Color Picker)

V kompakt pogledu ali 2D layout popupu klikni na barvni kanal fixture-a za odprtje barvnega kola:

- **Barvno kolo** — klikni/vleci za izbiro barve (Hue + Saturation)
- **Dimmer drsnik** — nastavi svetilnost
- **Predogled** — kvadrat z izbrano barvo + HEX koda
- **Paleta hitrih barv** — vnaprej definirane barve za hitro izbiro

### HSV barvno kolo — samodejno mapiranje kanalov

Barvno kolo pošilja HSV vrednosti (Hue, Saturation, Value) neposredno na ESP32, kjer se izvede pretvorba na strani strežnika. To prinaša naslednje prednosti:

- **ESP32 samodejno mapira HSV v ustrezne kanale** glede na profil fixture-a:
  - **RGB** — osnovna pretvorba HSV v rdeča (R), zelena (G), modra (B) kanale
  - **RGBW** — če ima fixture kanal za belo (`color_w`), se bela komponenta izloči iz nenasičenega dela barve. Rezultat: čistejši beli toni in daljša življenjska doba LED-ic
  - **Amber (`color_a`)** — če ima fixture amber kanal, se amber vrednost izpelje iz toplih tonov barve (oranžna/rumena regija)
  - **UV (`color_uv`)** — če ima fixture UV kanal, se UV intenziteta nastavi na podlagi modrega/vijoličnega odtenka (hue v območju 240°–300°)
- **Ena WebSocket poruka na spremembo barve** namesto 3–6 ločenih sporočil (eno za vsak kanal). To zmanjša obremenitev omrežja in zagotavlja, da se vsi kanali spremenijo simultano brez utripanja
- **Ni potrebe po ročnem nastavljanju** posameznih barvnih kanalov — preprosto izberite barvo na kolesu in ESP32 poskrbi za pravilno razdelitev med vse razpoložljive barvne kanale

#### Primer uporabe

Izberite 6-kanalni RGBAW fixture in izberite barvo na barvnem kolesu. ESP32 samodejno nastavi kanale R, G, B, A in W na pravilne vrednosti. Na primer: topla bela barva bo aktivirala beli kanal in amber kanal, medtem ko bodo RGB kanali nastavljeni na minimalno vrednost za fino korekcijo odtenka. Ni potrebe po ročnem nastavljanju vsakega barvnega kanala posebej!

---

## 7. XY Pad popup

Za natančno Pan/Tilt kontrolo na moving headih:

- **Canvas 200×200px** — horizontalna os = Pan, vertikalna os = Tilt
- **Klikni/vleci** — nastavi Pan in Tilt hkrati
- Prikazuje trenutne vrednosti (0–255) za oba kanala

### Čarobna palica (Magic Wand)

XY Pad popup vključuje gumb za čarobno palico, ki omogoča krmiljenje Pan/Tilt z žiroskopom telefona.

#### Kako deluje

Uporablja brskalnikov DeviceOrientation API za branje podatkov žiroskopa telefona. Nagibanje telefona krmili Pan (levo/desno) in Tilt (naprej/nazaj) izbranih fixture-ov.

#### Aktivacija

1. Odprite XY Pad popup (kliknite zeleno piko na fixture-u v 2D Layout pogledu)
2. Kliknite gumb s palico (ikona: wand/UFO)
3. Ob prvem kliku se izvede **kalibracija** — trenutni položaj telefona se nastavi kot sredinska (nevtralna) točka
4. Ikona palice prikaže aktivno stanje (spremenjena barva/slog)

#### Deaktivacija

- Kliknite gumb s palico ponovno, ali
- Zaprite XY Pad popup

#### Obseg gibanja

- Privzeto: **±50°** nagiba telefona se preslika na celoten obseg Pan/Tilt (0–255)
- Obseg je nastavljiv za bolj ali manj občutljivo krmiljenje

#### Najboljše prakse

- **Držite telefon ravno** pred seboj v višini prsi za najboljšo kontrolo
- **Majhni, premišljeni gibi** za natančno pozicioniranje
- Deluje na **vseh pametnih telefonih in tablicah** z žiroskopom
- **iOS zahteva HTTPS** povezavo (varnostna omejitev brskalnika Safari)
- **Deluje samo na mobilnih napravah** — na namiznih računalnikih brez žiroskopa funkcija ni na voljo

#### Primer uporabe

Odprite 2D Layout, kliknite na moving head fixture, kliknite zeleno piko za XY Pad popup, kliknite gumb s palico. Sedaj nagnite telefon levo/desno za Pan, naprej/nazaj za Tilt. Kot da s čarobno palico usmerjate reflektor!

---

## 8. Multiplayer sinhronizacija

Mixer podpira sočasno upravljanje z več naprav hkrati prek WebSocket povezave.

### Kako deluje

- **Več naprav** lahko krmili isti ESP32 sočasno — brez omejitev števila povezanih odjemalcev
- **Ob novi povezavi** naprava takoj prejme celotno trenutno stanje (vse faderje, barve, scene, blackout status itd.)
- **Vse spremembe se prenašajo v realnem času** na vse povezane naprave. Ko en uporabnik premakne fader, vsi ostali vidijo spremembo takoj
- **Ni konflikta** — zadnja sprememba vedno prevlada (last-write-wins). Če dva uporabnika hkrati spremenita isti kanal, se uporabi zadnja prejeta vrednost

### Primer uporabe

Svetlobni tehnik upravlja mixer na prenosnem računalniku z mišjo za natančno nastavljanje scen. DJ na telefonu krmili master dimmer in flash gumb med nastopom. Pomočnik na tablici v 2D Layout pogledu usmerja moving heade. Vsi trije vidijo in krmilijo isto stanje mixerja — spremembe enega se takoj odrazijo pri ostalih.

> **Opomba:** Vse naprave morajo biti povezane na isto WiFi omrežje kot ESP32. Priporočena je stabilna WiFi povezava za nemoteno sinhronizacijo.
