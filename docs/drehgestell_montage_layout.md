# Drehgestell-Montage & Sensor-Platinenlayout

## Modell: H0 Doppelstock-Steuerwagen DBbzfa 761 (Märklin 78479)
**Fokus:** Aufgabe 2 – Kompaktes Platinenlayout für den Sensorblock und mechanische Integration im hinteren Drehgestell.

---

## 1. Einbauort & Baufreiheit

* **Drehgestell:** Hinteres Drehgestell (Görlitz V Drehgestell ohne Mittelschleifer).
* **Achse:** **Innere Achse** (zum Wageninneren gerichtet).
* **Warum die innere Achse?**
  * Das vordere Drehgestell trägt den Mittelschleifer für die Gleiswechselspannung (Gefahr von Funkenflug und mechanischer Enge).
  * Die innere Achse des hinteren Drehgestells liegt direkt unter dem Wagenboden/Inneneinrichtung, wo Bohrungen für Kabel unsichtbar bleiben und maximale vertikale Baufreiheit herrscht.
  * Das H0-Radsatzinnenmaß (Abstand zwischen den Radscheiben) beträgt bei Märklin ca. **14,0 bis 14,2 mm**. Ein Sensorblock mit zwei A3144 benötigt nur ca. **9,5 mm** Breite und passt perfekt zwischen die Räder.

---

## 2. Platinen-Layout für den Sensorblock

Um die beiden A3144 stabil im Drehgestell zu fixieren, wird eine winzige Lochraster- oder Streifenrasterplatine (Standard-Raster $2{,}54\text{ mm}$, 4 Löcher breit × 3 Löcher hoch, ca. $10 \times 7{,}5\text{ mm}$) verwendet.

### Schaltbild auf der Mini-Platine:
Da beide Sensoren dieselbe Versorgungsspannung ($5\text{ V}$) und Masse ($GND$) benötigen, werden die Pins 1 und 2 direkt auf der Platine zusammengefasst:
* **Pin 1 (Sensor A) + Pin 1 (Sensor B)** $\rightarrow$ Gemeinsame $5\text{ V}$ Zuleitung (z. B. Rote Litze)
* **Pin 2 (Sensor A) + Pin 2 (Sensor B)** $\rightarrow$ Gemeinsame $GND$ Zuleitung (z. B. Schwarze Litze)
* **Pin 3 (Sensor A)** $\rightarrow$ Signal A (z. B. Gelbe Litze $\rightarrow$ Arduino Pin D2)
* **Pin 3 (Sensor B)** $\rightarrow$ Signal B (z. B. Grüne Litze $\rightarrow$ Arduino Pin D3)

### Bestückungs- und Lötplan (Raster 2,54 mm):

```
       Draufsicht (Bestückungsseite, Sensoren stehend / hängend):
       Sensoren zeigen mit der beschrifteten Vorderseite zur Achse!

            Spalte 1     Spalte 2     Spalte 3     Spalte 4
         +------------+------------+------------+------------+
  Reihe 1| [ VCC_A ]  | [ GND_A ]  | [ VCC_B ]  | [ GND_B ]  |  <-- Brücke VCC_A-VCC_B (5V)
         |  (Pin 1)   |  (Pin 2)   |  (Pin 1)   |  (Pin 2)   |  <-- Brücke GND_A-GND_B (GND)
         +------------+------------+------------+------------+
  Reihe 2|            | [ OUT_A ]  |            | [ OUT_B ]  |  <-- Pin 3 gebogen
         |            |  (Pin 3)   |            |  (Pin 3)   |
         +------------+------------+------------+------------+
  Reihe 3| Lötpad 5V  | Lötpad GND | Lötpad A   | Lötpad B   |  <-- 4 flexible Decoderlitzen
         +------------+------------+------------+------------+
               |            |            |            |
             (Rot)      (Schwarz)     (Gelb)       (Grün)
               v            v            v            v
           Arduino 5V  Arduino GND   Nano D2      Nano D3
```

> [!TIP]
> **Tipp für flachen Bauraum:** Die Beinchen der A3144 können vorsichtig um $90^\circ$ abgewinkelt werden, sodass die Platine flach unter dem Drehgestell-Querträger sitzt und die beiden Sensoren direkt senkrecht nach unten zur Achse ragen.

---

## 3. Magnetmontage auf der Achswelle

```
      Radsatz-Querschnitt (Blick von oben):

      Rad links                                               Rad rechts
      +---+                                                     +---+
      |   |==================[MAGNET]===========================|   |
      +---+                      ^                              +---+
                           2x1 mm Neodym
                           Südpol nach außen!
                                 |
                                 v
                         [ SENSOR A | SENSOR B ]
                              (Abstand: 1-2 mm)
```

1. **Vorbereitung:**
   * Achse an der Klebestelle mit Isopropanol oder Reinigungsbenzin gründlich entfetten.
2. **Ausrichtung:**
   * Vor dem Kleben sicherstellen, dass die im Breadboard-Test ermittelte **Südpol-Fläche nach außen** (in Richtung der Hall-Sensoren) zeigt!
3. **Kleben:**
   * Einen winzigen Tropfen Sekundenkleber-Gel (Cyanacrylat, z. B. Loctite 401/454) oder 2-Komponenten-Epoxidharz (UHU Plus Sofortfest) mittig auf die Achswelle geben.
   * Den Neodym-Magneten ($2 \times 1\text{ mm}$) aufsetzen und aushärten lassen.
   * *Prüfung:* Radsatz vorsichtig mit den Fingern drehen; der Rundlauf muss gewährleistet sein, ohne am Drehgestellrahmen zu schleifen.

---

## 4. Justage des Luftspalts (Sensor zu Magnet)

* **Ideal-Abstand:** **$1{,}0\text{ mm}$ bis maximal $2{,}0\text{ mm}$** zwischen Magnetoberfläche und Gehäusevorderseite der A3144.
* **Toleranz:**
  * Ist der Abstand $> 2{,}5\text{ mm}$, reicht das Magnetfeld des $2\times 1\text{ mm}$ Scheibenmagneten eventuell nicht mehr aus, um die Schaltschwelle ($B_{OP} \approx 100-200\text{ Gauss}$) zuverlässig zu triggern.
  * Ist der Abstand $< 0{,}5\text{ mm}$, besteht bei Gleisunebenheiten oder Achsspiel das Risiko mechanischen Kontakts.
* **Test:** Nach dem Einbau Drehgestell von Hand drehen und mit dem Testsketch `breadboard_test.ino` kontrollieren, ob bei jeder Radumdrehung exakt 1 Impuls pro Sensor gezählt wird.

---

## 5. Kabelführung durch den Drehzapfen

Das Drehgestell muss in engen Radien (z. B. Märklin R1 = $360\text{ mm}$) frei und ohne mechanischen Widerstand ausschwenken können.

```
       Wagenboden / Drehzapfen (Schnitt):

       +---------------------------------------------+
       |             Wagen-Innenraum                 |
       |        (Arduino Nano & Elektronik)          |
       +--------------------+   +--------------------+
                            |   |  <-- Zentrale Bohrung (ca. 2,0 mm)
                            |   |      durch Drehzapfen
                            |   |
                   ~~~~~ S-Schleife ~~~~~ (AWG 36 Decoderlitze)
                            |   |
       +--------------------+   +--------------------+
       |       Drehgestell-Rahmen (Görlitz V)        |
       |       [Sensor-Platine]                      |
       +---------------------------------------------+
```

### Richtlinien für die Verkabelung:
1. **Drahtwahl:** Verwende ultra-flexible **Decoderlitze AWG 36** (Außendurchmesser ca. $0{,}5\text{ mm}$, feinstdrähtig) oder Kupferlackdraht ($0{,}15\text{ mm}$). Niemals starre Schaltdrähte im Drehgestellbereich einsetzen!
2. **Zentrale Durchführung:** Die 4 Litzen werden gebündelt direkt durch das Zentrum des Drehzapfens nach oben in den Wagenboden geführt.
3. **S-Schleife:** Vor dem Fixieren im Wageninneren eine kleine Entlastungsschlaufe (S-Schleife) legen.
4. **Freigängigkeitstest:**
   * Drehgestell von Hand bis zum maximalen Anschlag nach links und rechts schwenken.
   * Das Drehgestell muss von selbst federleicht in die Mittelstellung zurückgleiten, ohne durch die Litzen blockiert oder vorgespannt zu werden.
