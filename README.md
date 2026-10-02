# Autonomer Fahrtrichtungs-Lichtwechsel für H0-Steuerwagen (Märklin 78479)

Dieses Repository enthält die vollständige Firmware, Schaltpläne, Montageanleitungen und Test-Werkzeuge für die Nachrüstung eines **autonomen, fahrtrichtungsabhängigen Lichtwechsels** in den H0 Doppelstock-Steuerwagen **DBbzfa 761** (aus der Märklin-Ergänzungspackung 78479).

---

## 🚂 Das Projekt auf einen Blick

* **Ziel:** Automatischer Lichtwechsel zwischen **3× warmweißem Spitzensignal** (Fahrt voraus) und **2× rotem Schlusslicht** (Lok zieht, Steuerwagen am Zugschluss).
* **Autonomes Prinzip:** Vollkommen unabhängig von Digitalzentralen, DCC-Befehlen, Funktionsdecodern oder mechanisch schwergängigen Radschleifern/Schleppschaltern.
* **Sensorik:** 2× unipolare digitale Hall-Sensoren **A3144** an der inneren Achse des hinteren Drehgestells erfassen die Drehung eines winzigen **$2 \times 1\text{ mm}$ Neodym-Magneten** (Südpol nach außen).
* **Steuerung:** **Arduino Nano** (ATmega328P, 5 V / 16 MHz).
* **Stromversorgung & Flackerschutz:** Gleiswechselspannung $\rightarrow$ Mini-Brückengleichrichter (DB107S) $\rightarrow$ Step-Down (5,3 V) $\rightarrow$ 1N5819 Schottky-Diode $\rightarrow$ Puffer-Elkos ($200 - 300\text{ µF}$) $\rightarrow$ Arduino 5V.

---

## 📁 Repository-Struktur

```
steuerwagen-Fahrtrichtung/
├── firmware/
│   ├── ws2811_test/
│   │   └── ws2811_test.ino               # Standalone-Test für WS2811 (ohne Hall-Sensoren)
│   ├── breadboard_test/
│   │   └── breadboard_test.ino           # Interaktiver Test- & Diagnosesketch für Steckbrett
│   └── steuerwagen_lichtwechsel/
│       └── steuerwagen_lichtwechsel.ino   # Haupt-Produktionsfirmware mit Soft-Fade & Entprellung
├── docs/
│   ├── schaltplan_und_elektronik.md       # Vollständiger Schaltplan, Pufferung & Stückliste (BOM)
│   ├── breadboard_test_anleitung.md       # Schritt-für-Schritt Steckbrett-Anleitung & Polaritätscheck
│   └── drehgestell_montage_layout.md      # Platinenlayout (10x7.5mm), Achsmontage & Litzenführung
├── LICENSE                                # Lizenz
└── README.md                              # Dieses Dokument
```

---

## ⚡ Pin-Mapping (Arduino Nano)

| Nano Pin | Funktion | Beschreibung |
| :---: | :--- | :--- |
| **D2** | Sensor A | Signal Hall-Sensor A (`INT0`, Hardware-Interrupt, `INPUT_PULLUP`) |
| **D3** | Sensor B | Signal Hall-Sensor B (`INT1`, Hardware-Interrupt, `INPUT_PULLUP`) |
| **D6** | WS2811 DIN | Adressierbarer LED-Treiber: **Kanal G** = Vorwärts (Weiß), **Kanal R** = Rückwärts (Rot) |
| **5V** | Betriebsspannung | Geregelte & gepufferte 5,0 V DC (hinter 1N5819 Schottky-Diode) |
| **GND**| Gemeinsame Masse | Bezugspotenzial für Step-Down, Arduino, Sensoren und WS2811 |

---

## 🛠️ Schnellstart & Nächste Schritte

### Schritt 1: Breadboard-Test & Magnetpolung
Vor dem finalen Einbau in den Steuerwagen werden die Sensoren und die Richtungslogik auf dem Steckbrett geprüft:
1. Schaltung gemäß [breadboard_test_anleitung.md](docs/breadboard_test_anleitung.md) aufbauen.
   * **Wichtig:** Sensor A und B in **getrennte Spalten/Reihen** stecken, um Kurzschlüsse zu verhindern!
2. Den Sketch [breadboard_test.ino](firmware/breadboard_test/breadboard_test.ino) auf den Arduino Nano flashen.
3. Im Serial Monitor (**115200 Baud**) prüfen:
   * Magnet mit **Südpol** an die beschriftete Seite von A3144 halten $\rightarrow$ sofortige Erkennung.
   * Wischen von A nach B $\rightarrow$ WS2811 schaltet **Kanal G**, Richtungsstatus: `VORWÄRTS`.
   * Wischen von B nach A $\rightarrow$ WS2811 schaltet **Kanal R**, Richtungsstatus: `RÜCKWÄRTS`.
   * Über die Tasten `+` / `-` oder `0`–`9` die Helligkeit der Kanäle justieren.

### Schritt 2: Drehgestell-Sensorblock vorbereiten
1. Mini-Platine (ca. $10 \times 7{,}5\text{ mm}$, 4×3 Lochraster) gemäß [drehgestell_montage_layout.md](docs/drehgestell_montage_layout.md) bestücken.
2. Neodym-Magnet ($2\times 1\text{ mm}$) mit markiertem Südpol nach außen mittig auf die innere Achswelle des hinteren Drehgestells kleben.
3. Sensorblock mit ca. $1{,}0 - 2{,}0\text{ mm}$ Luftspalt über der Achse montieren.
4. 4 hochflexible Decoderlitzen (AWG 36) mit S-Schlaufe durch den Drehzapfen nach oben führen.

### Schritt 3: Stromversorgung & Produktions-Firmware
1. Gleichrichter, Step-Down (5,3 V), Diode (1N5819) und Puffer-Elkos ($2-3\times 100\text{ µF}$) gemäß [schaltplan_und_elektronik.md](docs/schaltplan_und_elektronik.md) verlöten.
2. Den Produktions-Sketch [steuerwagen_lichtwechsel.ino](firmware/steuerwagen_lichtwechsel/steuerwagen_lichtwechsel.ino) aufspielen.
3. Im Wageninneren verdrahten, zuschneiden und den Steuerwagen aufgleisen!

---

## 💡 Technische Highlights der Firmware

* **Duale Richtungserkennung:** Unterstützt sowohl klassische Quadratur-Überlappung (`digitalRead(D3)` bei fallender Flanke A) als auch **sequenzbasierte Flankenauswertung** über `INT0` & `INT1`. Dadurch funktioniert die Erkennung auch dann 100 % zuverlässig, wenn der winzige $2\text{ mm}$ Magnet die Sensoren zeitlich nacheinander statt überlappend auslöst.
* **Prellschutz & HF-Filter:** Software-Entprellzeit von 8 ms verwirft HF-Störungen und Schaltfunken vom Gleis, ohne bei Vorbild-Höchstgeschwindigkeiten (160–200 km/h) Impulse zu verlieren.
* **Sanftes Überblenden (Soft-Fade):** Vorbildgerechtes Auf- und Abblenden beim Fahrtrichtungswechsel wie bei echten Glühlampen.
* **Dauerhafte Zustandsspeicherung:** Im Stillstand (auch bei kurzen Gleisunterbrechungen dank Elko-Pufferung) bleibt das Licht unverändert aktiv.
