# Breadboard-Testanleitung (Schritt-für-Schritt)

## Projekt: Autonomer Fahrtrichtungs-Lichtwechsel für H0-Steuerwagen (Märklin 78479)
**Fokus:** Aufgabe 1 – Überprüfung der A3144-Sensoren, Magnetpolung, Richtungslogik & LED-Helligkeit auf dem Steckbrett.

---

## 1. Pinbelegung des Hall-Sensors A3144

Der A3144 ist ein unipolarer digitaler Hall-Sensor mit Open-Drain-Ausgang (Active-LOW).

```
   TO-92 Gehäuse
   (Draufsicht auf die flache, BESCHRIFTETE Vorderseite)
       +-------+
       | A3144 |
       |  E042 |
       +-------+
        |  |  |
        |  |  |
        1  2  3
        |  |  +---> Pin 3: OUTPUT (Signal OUT -> an D2 bzw. D3)
        |  +------> Pin 2: GND (Masse -> an Arduino GND)
        +---------> Pin 1: VCC (Betriebsspannung -> an Arduino 5V)
```

> [!CAUTION]
> **Wichtig:** Der A3144 schaltet ausschließlich, wenn der **SÜDPOL** des Magneten auf die **beschriftete Vorderseite** gerichtet wird. Der Nordpol wird ignoriert!

---

## 2. Der „Getrennte-Spalten-Fehler“ auf dem Steckbrett

Auf einem Standard-Breadboard sind die 5 Löcher einer Reihe (z. B. Reihe 15: Spalten `a`-`b`-`c`-`d`-`e`) intern leitend miteinander verbunden.

> [!WARNING]
> Werden die beiden A3144 einfach nebeneinander in dieselben Reihen gesteckt (z. B. Sensor A in Spalte `b`, Sensor B in Spalte `c`), sind Pin 1 mit Pin 1, Pin 2 mit Pin 2 und **Pin 3 (Signal A) mit Pin 3 (Signal B) kurzgeschlossen**! Das System kann dann keine Flanken mehr unterscheiden.

### Richtige Steckplatz-Varianten auf dem Breadboard:

#### Variante A: Versetzte Reihen (Empfohlen)
* **Sensor A:**
  * Pin 1 (VCC): Reihe 10, Spalte `c`
  * Pin 2 (GND): Reihe 11, Spalte `c`
  * Pin 3 (OUT): Reihe 12, Spalte `c` $\rightarrow$ Drahtbrücke zu Arduino **D2**
* **Sensor B:**
  * Pin 1 (VCC): Reihe 15, Spalte `c`
  * Pin 2 (GND): Reihe 16, Spalte `c`
  * Pin 3 (OUT): Reihe 17, Spalte `c` $\rightarrow$ Drahtbrücke zu Arduino **D3**

#### Variante B: Über den Mittelgraben
* **Sensor A** links vom Mittelsteg (Spalten `c`, `d`, `e`).
* **Sensor B** rechts vom Mittelsteg (Spalten `f`, `g`, `h`).

---

## 3. Kompletter Steckplan auf dem Breadboard

```
+---------------------------------------------------------------------------------+
|                                 BREADBOARD                                      |
+---------------------------------------------------------------------------------+
                      [+] Rote Versorgungsleiste  = Arduino 5V
                      [-] Blaue Versorgungsleiste = Arduino GND

  Sensor A (A3144):
  Reihe 10: Pin 1 (VCC) -----------> Verbinden mit [+] 5V
  Reihe 11: Pin 2 (GND) -----------> Verbinden mit [-] GND
  Reihe 12: Pin 3 (OUT) -----------> Verbinden mit Arduino Pin D2

  Sensor B (A3144):
  Reihe 15: Pin 1 (VCC) -----------> Verbinden mit [+] 5V
  Reihe 16: Pin 2 (GND) -----------> Verbinden mit [-] GND
  Reihe 17: Pin 3 (OUT) -----------> Verbinden mit Arduino Pin D3

  LED Weiß (Frontlicht):
  Reihe 22: Anode (+) (langes Bein) -> 1 kΩ Widerstand -> Arduino Pin D4
  Reihe 23: Kathode (-) (kurzes Bein) -> Verbinden mit [-] GND

  LED Rot (Schlusslicht):
  Reihe 26: Anode (+) (langes Bein) -> 1 kΩ Widerstand -> Arduino Pin D5 (PWM)
  Reihe 27: Kathode (-) (kurzes Bein) -> Verbinden mit [-] GND
```

---

## 4. Magnet-Polarität prüfen (Südpol identifizieren)

Da der $2 \times 1\text{ mm}$ Neodym-Magnet sehr klein ist, muss vor dem Festkleben zweifelsfrei der **Südpol** markiert werden:

1. **Methode mit dem Arduino-Testsketch:**
   * Lade den Sketch `firmware/breadboard_test/breadboard_test.ino` auf den Nano.
   * Öffne den Serial Monitor bei **115200 Baud**.
   * Halte eine Flachseite des Magneten ca. 1–2 mm vor die beschriftete Vorderseite von Sensor A.
   * **Reaktion:**
     * Zeigt der Serial Monitor sofort `[MAGNET ERKANNT (LOW)]` an, zeigt der **SÜDPOL** gerade zum Sensor!
     * Passiert absolut gar nichts: Drehe den Magneten um $180^\circ$ (auf die andere Flachseite). Nun muss der Sensor schalten.
2. **Markierung:**
   * Markiere die erkannte Südpol-Seite sofort mit einem feinen wasserfesten Filzstift (z. B. roter Punkt). Diese Seite muss später am Drehgestell nach **außen** zeigen!

---

## 5. Durchführung des Funktionstests

1. **Serial Monitor öffnen:**
   * Baudrate: **115200**
   * Zeilenende: Sowohl NL als auch CR (Both NL & CR).
2. **Simulierte Vorwärtsfahrt:**
   * Führe den Magneten zügig von **Sensor A nach Sensor B** vorbei.
   * **Ergebnis:**
     * Front-LED (Weiß) leuchtet auf.
     * Rote LED erlischt.
     * Ausgabe: `[TRIGGER] VORWAERTS [Front Weiss] | Methode 1: VORWAERTS | Zaehler A: 1 B: 1`.
3. **Simulierte Rückwärtsfahrt:**
   * Führe den Magneten in Gegenrichtung von **Sensor B nach Sensor A** vorbei.
   * **Ergebnis:**
     * Front-LED erlischt.
     * Rote LED leuchtet auf.
     * Ausgabe: `[TRIGGER] RUECKWAERTS [Schluss Rot] | Zaehler A: 2 B: 2`.
4. **Stillstandstest:**
   * Nimm den Magneten ganz weg. Der Zustand (Weiß oder Rot) muss unbegrenzt stabil erhalten bleiben!

---

## 6. Serielle Steuerbefehle für den Feinabgleich

Im Serial Monitor kannst du über die Tastatur direkte Befehle senden:

| Taste | Aktion | Zweck |
| :---: | :--- | :--- |
| `s` | Statusbericht | Zeigt aktuelle Pinpegel (HIGH/LOW) und Zählerstände an |
| `w` | Frontlicht Weiß erzwingen | Test der weißen LEDs |
| `r` | Schlusslicht Rot erzwingen | Test der roten LEDs |
| `+` / `-` | PWM-Helligkeit erhöhen/senken | Feineinstellung der Helligkeit des roten Schlusslichts |
| `0` .. `9` | Direkte Helligkeitsstufe 0–9 | Ermittlung des perfekten Dimmwerts (z. B. `5` = ~50 %, `7` = ~70 %) |
| `c` | Zähler auf 0 | Bereinigt die Auslösestatistik |
| `h` / `?` | Hilfe anzeigen | Zeigt das Befehlsmenü erneut an |

> [!TIP]
> Sobald du den idealen Helligkeitswert für das rote Schlusslicht ermittelt hast (z. B. Stufe 6 = PWM 170), trage diesen Wert in der Haupt-Firmware `steuerwagen_lichtwechsel.ino` bei der Konstante `RED_PWM_BRIGHTNESS` ein.
