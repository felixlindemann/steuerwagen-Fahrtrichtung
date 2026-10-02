# Schaltplan & Elektronik-Dokumentation

## Projekt: Autonomer Fahrtrichtungs-Lichtwechsel für H0-Steuerwagen (Märklin 78479)
**Modell:** H0 Doppelstock-Steuerwagen DBbzfa 761  
**Mikrocontroller:** Arduino Nano (ATmega328P, 5V / 16 MHz)

---

## 1. Blockschaltbild

```
+-----------------------------------------------------------------------------------+
| GLEIS (Märklin Digital / Mittelleiter rot + Achsmasse braun)                      |
+-----------------------------------------------------------------------------------+
       |                                          |
       v (Rot: Mittelschleifer)                   v (Braun: Rad-/Achsmasse)
+-----------------------------------------------------------------------------------+
| MINI-BRÜCKENGLEICHRICHTER (z. B. DB107S / MB6S, 1 A / 1000 V)                     |
+-----------------------------------------------------------------------------------+
       | DC (+)                                   | DC (-)
       v                                          v
+-----------------------------------------------------------------------------------+
| STEP-DOWN BUCK CONVERTER (LM2596 / MP1584 / Mini360)                              |
| Eingestellt auf: 5,3 V DC Ausgang                                                 |
+-----------------------------------------------------------------------------------+
       | Out (+)                                  | Out (-)
       v                                          |
+------------------------------------+            |
| ENTKOPPLUNG & LADESCHUTZ           |            |
| 1N5819 Schottky-Diode (Vf ~0,3 V)  |            |
| (Optional in Reihe: 10 Ω Ladeschutz)|           |
+------------------------------------+            |
       | Saubere 5,0 V DC                         | Gemeinsame Masse
       +--------------------+                     |
       |                    |                     |
       v                    v                     |
+---------------+   +-----------------------+     |
| ARDUINO NANO  |   | PUFFER-ELKOS          |     |
| Pin: 5V       |   | 2x - 3x 100 µF / 25 V |<----+
| Pin: GND <----+   | parallel gegen GND    |
+---------------+   +-----------------------+
    |    |    |
    |    |    +---> D6 (DIN) ----> WS2811 (Pin 6 DIN)
    |    |                         ├── OUTG (Pin 2) ---> Kathode (-) Vorwärts-LEDs (Weiß)
    |    |                         └── OUTR (Pin 1) ---> Kathode (-) Rückwärts-LEDs (Rot)
    |    +--------> D3 (INT1)<--- A3144 Hall-Sensor B (Signal OUT)
    +-------------> D2 (INT0)<--- A3144 Hall-Sensor A (Signal OUT)
```

---

## 2. Detaillierter Schaltplan

```
                          1N5819
    Step-Down (+) 5.3V ---->|----+-----------------------+--------------> Arduino 5V
                                 |                       |
                                === 2-3x 100µF / 25V     +--------------> WS2811 VDD (Pin 8)
                                === (Parallel)           +--------------> A3144 (A) VCC (Pin 1)
                                 |                       |
    Step-Down (-) GND -----------+-----------------------+--------------> A3144 (B) VCC (Pin 1)
                                 |                       |
                                 +-----------------------+--------------> Arduino GND
                                 |                       |
                                 +-----------------------+--------------> WS2811 GND (Pin 4)
                                 |                       |
                                 +-----------------------+--------------> A3144 (A) GND (Pin 2)
                                 |                       |
                                 +-----------------------+--------------> A3144 (B) GND (Pin 2)
                                 |
                                 |
    Arduino D2 (INT0) <----------+-------------------------------------- A3144 (A) OUT (Pin 3)
    (INPUT_PULLUP aktiv)

    Arduino D3 (INT1) <------------------------------------------------- A3144 (B) OUT (Pin 3)
    (INPUT_PULLUP aktiv)

    Arduino D6 --------------------------------------------------------> WS2811 DIN (Pin 6)

    +5V (Versorgung) ------------+-----------------------+
                                 |                       |
                                 v Anode (+)             v Anode (+)
                         [Vorwärts-LEDs]         [Rückwärts-LEDs]
                                 | Kathode (-)           | Kathode (-)
                                 v                       v
    WS2811 OUTG (Pin 2) <--------+                       |
    (Kanal G: Vorwärts aktiv)                            |
                                                         |
    WS2811 OUTR (Pin 1) <--------------------------------+
    (Kanal R: Rückwärts aktiv)
```

---

## 3. Schaltungsanalyse: Pufferung, Entkopplung & Flackerschutz

### A. Schutz vor Rückspeisung in den Step-Down
* Wenn der Zug über Weichenstraßen oder verschmutzte Schienen fährt, bricht die Gleisspannung für wenige Millisekunden ein.
* Die **Schottky-Diode 1N5819** liegt in Durchlassrichtung zwischen Step-Down (+) und dem `5V`-Pin des Arduino Nano.
* **Vorteil:** Die Kathode der Diode versorgt sowohl den Nano als auch die Puffer-Elkos. Fällt die Eingangsspannung am Step-Down ab, verhindert die Diode, dass sich die Elkos rückwärts über den internen Spannungsteiler des Step-Down-Reglers entladen. Die gesamte Ladung steht exklusiv dem Arduino und den Sensoren/LEDs zur Verfügung.
* Durch die geringe Flussspannung der Schottky-Diode ($V_F \approx 0{,}3\text{ V}$) sinkt die Ausgangsspannung von 5,3 V exakt auf die nominellen **5,0 V** für den ATmega328P.

### B. Dimensionierung der Puffer-Kondensatoren
* **Kapazität:** 2× bis 3× $100\text{ µF} / 25\text{ V}$ parallel geschaltet (Gesamtkapazität $200\text{ µF} - 300\text{ µF}$).
* **Strombedarf der Schaltung:**
  * Arduino Nano im Normalbetrieb: ca. $15\text{ mA} - 20\text{ mA}$
  * 2× A3144 Ruhestrom: ca. $2 \times 4\text{ mA} = 8\text{ mA}$
  * LEDs (Front 3× Weiß bei 1 kΩ an 5V: ca. $3 \times 1{,}8\text{ mA} \approx 5{,}4\text{ mA}$; bzw. Schluss 2× Rot gedimmt: ca. $3\text{ mA}$)
  * **Gesamtstromaufnahme:** ca. $30\text{ mA} - 35\text{ mA}$.
* **Pufferzeit:**
  $$\Delta t = \frac{C \cdot \Delta V}{I} = \frac{300\text{ µF} \cdot 1{,}5\text{ V}}{35\text{ mA}} \approx 13\text{ ms}$$
  Der ATmega328P läuft stabil bis herab zu 3,5 V / 16 MHz bzw. 2,7 V. Eine Pufferung von 10–15 ms reicht aus, um typische Mikrounterbrechungen an Weichenzungen unsichtbar zu überbrücken.

### C. Sanftes Laden (Inrush-Current)
* Da $300\text{ µF}$ eine moderate Kapazität darstellen, entsteht beim Aufgleisen kein gefährlicher Funke.
* Falls ein Ladeschutzwiderstand ($10\text{ }\Omega$) verwendet wird, empfiehlt sich dieser **vor** der Diode in Reihe oder mit einer separaten Lade-/Entladediode, damit bei Entladung kein Spannungsabfall am Widerstand entsteht.

---

## 4. LED-Dimensionierung & Vorwiderstände

* **Frontbeleuchtung (Weiß):**
  * 3× SMD-LEDs (z. B. Bauform 0603 oder 0805, warmweiß, $V_F \approx 3{,}0\text{ V}$).
  * Jede LED besitzt einen **eigenen 1 kΩ Vorwiderstand** (kein gemeinsamer Vorwiderstand, um gleichmäßige Helligkeit zu garantieren).
  * Strom pro LED: $I_F = \frac{5{,}0\text{ V} - 3{,}0\text{ V}}{1000\text{ }\Omega} = 2{,}0\text{ mA}$.
  * $2\text{ mA}$ ist bei modernen hocheffizienten SMD-LEDs ideal: absolut blendfrei, vorbildgerechte Helligkeit, minimale Erwärmung und extrem geringer Pufferverbrauch.

* **Schlussbeleuchtung (Rot):**
  * 2× SMD-LEDs (rot, $V_F \approx 1{,}9\text{ V}$).
  * Jede LED besitzt einen **eigenen 1 kΩ Vorwiderstand**.
  * Strom pro LED bei 100 % Duty Cycle: $I_F = \frac{5{,}0\text{ V} - 1{,}9\text{ V}}{1000\text{ }\Omega} = 3{,}1\text{ mA}$.
  * Da rote LEDs bei 3 mA oft noch zu grell leuchten, wird Pin **D5 per Hardware-PWM** angesteuert. Ein PWM-Wert von 140–180 erzeugt ein sattes, vorbildgerechtes Zugschlusslicht.

---

## 5. Stückliste (BOM)

| Pos. | Bauteil | Typ / Spezifikation | Anzahl | Bemerkung |
| :--- | :--- | :--- | :---: | :--- |
| 1 | Mikrocontroller | Arduino Nano V3 (ATmega328P, 5V / 16MHz) | 1 | Mit Mini- oder USB-C-Buchse |
| 2 | Hall-Sensor | A3144 (unipolar, digital, Open-Drain) | 2 | TO-92 Gehäuse |
| 3 | Neodym-Magnet | N45/N52 Scheibenmagnet $2 \times 1\text{ mm}$ | 1 | Südpol nach außen auf Achse kleben |
| 4 | Gleichrichter | DB107S (oder MB6S) Mini-Brückengleichrichter | 1 | 1 A / 1000 V |
| 5 | Step-Down Modul | LM2596 oder MP1584 Mini Step-Down | 1 | Auf 5,3 V eingestellt |
| 6 | Diode | 1N5819 (Schottky) | 1 | $V_F \approx 0{,}3\text{ V}$, 1 A |
| 7 | Widerstand | 10 Ω / 0,25 W (Metallschicht) | 1 | Ladeschutz |
| 8 | Widerstände | 1 kΩ / 0,25 W | 5 | Vorwiderstände (3x Weiß, 2x Rot) |
| 9 | Kondensatoren | 100 µF / 25 V (radial, mini) | 2–3 | Pufferung am 5V-Pin |
| 10 | SMD-LEDs Weiß | Bauform 0603 oder 0805 warmweiß | 3 | Spitzenlicht |
| 11 | SMD-LEDs Rot | Bauform 0603 oder 0805 rot | 2 | Schlusslicht |
| 12 | Litze | Flexible Decoderlitze (AWG 36 / 0,05 mm²) | 4 Farben | Für Drehgestell-Durchführung |
