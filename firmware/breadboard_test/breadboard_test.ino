/* =====================================================================================
 * Projekt:     Autonomer Fahrtrichtungs-Lichtwechsel für H0-Steuerwagen (Märklin 78479)
 * Datei:       breadboard_test.ino
 * Beschreibung:Interaktiver Diagnose- & Test-Sketch für den Breadboard-Aufbau.
 *              - Überprüfung der beiden A3144 Hall-Sensoren (Pinbelegung, Funktion)
 *              - Testen der Magnetpolung (Reaktion auf Südpol an der Beschriftungsseite)
 *              - Echtzeit-Auswertung beider Richtungserkennungs-Methoden:
 *                  * Methode 1: Klassische Quadratur (A fallend -> B Pegelabfrage)
 *                  * Methode 2: Sequenzerkennung (A vor B vs. B vor A)
 *              - Ansteuerung eines WS2811 LED-Treibers an Pin D6:
 *                  * Vorwärtsbetrieb:  Kanal G aktiv (Pin G belegt, z. B. Spitzenlicht weiß)
 *                  * Rückwärtsbetrieb: Kanal R aktiv (Pin R belegt, z. B. Schlusslicht rot)
 *              - Serielle Befehle zur Helligkeitsjustierung und Simulation
 * Baudrate:    115200 Baud
 * =====================================================================================
 */

#include <Arduino.h>
#include <Adafruit_NeoPixel.h>

// --- Pin- & WS2811-Definitionen ---
const uint8_t PIN_HALL_A = 2; // Hardware-Interrupt INT0
const uint8_t PIN_HALL_B = 3; // Hardware-Interrupt INT1
const uint8_t PIN_WS2811 = 6; // WS2811 Datenleitung DIN (Steuerung G=Vorwärts, R=Rückwärts)
const uint8_t NUM_PIXELS = 1; // 1x WS2811 Controller-IC

// WS2811 Konfiguration
// Standard-WS2811 ICs erwarten Daten im RGB-Format (OUTR -> Byte 0, OUTG -> Byte 1, OUTB -> Byte 2)
#define WS2811_TYPE (NEO_RGB + NEO_KHZ800)
Adafruit_NeoPixel ws2811(NUM_PIXELS, PIN_WS2811, WS2811_TYPE);

// --- Richtungs-Definitionen ---
enum Direction {
  DIR_UNKNOWN = 0,
  DIR_FORWARD, // Vorwärts: WS2811 Kanal G aktiv
  DIR_REVERSE  // Rückwärts: WS2811 Kanal R aktiv
};

// --- Globale Variablen für Interrupts & Sensorik ---
// Startet zu Testzwecken mit Vorwärts (Kanal G)
volatile Direction currentDirection = DIR_FORWARD;
volatile unsigned long lastTriggerTimeA = 0;
volatile unsigned long lastTriggerTimeB = 0;
volatile uint32_t triggerCountA = 0;
volatile uint32_t triggerCountB = 0;
volatile bool newTriggerEvent = false;
volatile Direction detectedByMethod1 = DIR_UNKNOWN; // Quadratur-Pegel
volatile Direction detectedByMethod2 = DIR_UNKNOWN; // Sequenz-Reihenfolge
volatile unsigned long eventDeltaMs = 0;

// Entprell-Zeitfenster in Millisekunden (bei H0-Höchstgeschwindigkeit ca. 8 ms Mindestzeit)
const unsigned long DEBOUNCE_MS = 8;
// Maximales Zeitfenster (ms) zwischen Sensor B und A für eine zusammenhängende Sequenz (gemessen ~75-80 ms)
const unsigned long SEQUENCE_TIMEOUT_MS = 600;

// Helligkeitswerte für die Kanäle G (Vorwärts) und R (Rückwärts) (0 .. 255)
uint8_t brightnessG = 255;
uint8_t brightnessR = 255;

// --- Automatische Wechselschaltung zu Testzwecken (2s Takt) ---
bool autoToggleEnabled = true;
const unsigned long AUTO_TOGGLE_INTERVAL_MS = 2000; // 2 Sekunden
unsigned long lastToggleTime = 0;

// Letzter bekannter Pin-Zustand für Live-Monitor
int lastPinA = HIGH;
int lastPinB = HIGH;

// =====================================================================================
// INTERRUPT SERVICE ROUTINES (ISRs)
// =====================================================================================

// ISR für Sensor A (Pin D2, INT0)
void isr_hall_a() {
  unsigned long now = millis();
  if (now - lastTriggerTimeA < DEBOUNCE_MS) {
    return; // Prellen / Rauschen ignorieren
  }

  int stateB = digitalRead(PIN_HALL_B);

  // 1. Überlappung: Wenn B bereits aktiv LOW ist -> B war zuerst -> B -> A = VORWÄRTS
  if (stateB == LOW) {
    detectedByMethod1 = DIR_FORWARD;
    currentDirection = DIR_FORWARD;
    lastTriggerTimeA = 0;
    lastTriggerTimeB = 0;
    triggerCountA++;
    newTriggerEvent = true;
    return;
  }

  // 2. Sequenz: Sensor B hat kurz zuvor ausgelöst -> Sequenz B -> A = VORWÄRTS
  if (lastTriggerTimeB > 0 && (now - lastTriggerTimeB) < SEQUENCE_TIMEOUT_MS) {
    detectedByMethod2 = DIR_FORWARD;
    eventDeltaMs = now - lastTriggerTimeB;
    currentDirection = DIR_FORWARD;
    lastTriggerTimeA = 0;
    lastTriggerTimeB = 0;
    triggerCountA++;
    newTriggerEvent = true;
    return;
  }

  // B war nicht zuvor aktiv -> A startet eventuell A -> B
  lastTriggerTimeA = now;
  triggerCountA++;
  newTriggerEvent = true;
}

// ISR für Sensor B (Pin D3, INT1)
void isr_hall_b() {
  unsigned long now = millis();
  if (now - lastTriggerTimeB < DEBOUNCE_MS) {
    return; // Prellen ignorieren
  }

  int stateA = digitalRead(PIN_HALL_A);

  // 1. Überlappung: Wenn A bereits aktiv LOW ist -> A war zuerst -> A -> B = RÜCKWÄRTS
  if (stateA == LOW) {
    detectedByMethod1 = DIR_REVERSE;
    currentDirection = DIR_REVERSE;
    lastTriggerTimeA = 0;
    lastTriggerTimeB = 0;
    triggerCountB++;
    newTriggerEvent = true;
    return;
  }

  // 2. Sequenz: Sensor A hat kurz zuvor ausgelöst -> Sequenz A -> B = RÜCKWÄRTS
  if (lastTriggerTimeA > 0 && (now - lastTriggerTimeA) < SEQUENCE_TIMEOUT_MS) {
    detectedByMethod2 = DIR_REVERSE;
    eventDeltaMs = now - lastTriggerTimeA;
    currentDirection = DIR_REVERSE;
    lastTriggerTimeA = 0;
    lastTriggerTimeB = 0;
    triggerCountB++;
    newTriggerEvent = true;
    return;
  }

  // A war nicht zuvor aktiv -> B startet eventuell B -> A
  lastTriggerTimeB = now;
  triggerCountB++;
  newTriggerEvent = true;
}

// =====================================================================================
// HILFSFUNKTIONEN
// =====================================================================================

void setWS2811(uint8_t r, uint8_t g, uint8_t b = 0) {
  ws2811.setPixelColor(0, r, g, b);
  ws2811.show();
}

void applyLighting() {
  if (currentDirection == DIR_FORWARD) {
    // Vorwärts: Kanal G an (Spitzenlicht), Kanal R aus, Kanal B aus
    setWS2811(0, brightnessG, 0);
    digitalWrite(LED_BUILTIN, HIGH);
  } else if (currentDirection == DIR_REVERSE) {
    // Rückwärts: Kanal R an (Schlusslicht), Kanal G aus, Kanal B aus
    setWS2811(brightnessR, 0, 0);
    digitalWrite(LED_BUILTIN, LOW);
  }
}

void printDirection(Direction d) {
  switch (d) {
  case DIR_FORWARD:
    Serial.print(F("VORWAERTS [WS2811 Kanal G aktiv]"));
    break;
  case DIR_REVERSE:
    Serial.print(F("RUECKWAERTS [WS2811 Kanal R aktiv]"));
    break;
  default:
    Serial.print(F("UNBEKANNT"));
    break;
  }
}

void printBanner() {
  Serial.println(F("\n========================================================"));
  Serial.println(F("  H0-STEUERWAGEN (Märklin 78479) - BREADBOARD TEST"));
  Serial.println(F("  Autonomer Fahrtrichtungs-Lichtwechsel mit WS2811"));
  Serial.println(F("========================================================"));
  Serial.println(F("Pin-Belegung Arduino Nano:"));
  Serial.println(F("  D2 : Hall-Sensor A (INT0, INPUT_PULLUP)"));
  Serial.println(F("  D3 : Hall-Sensor B (INT1, INPUT_PULLUP)"));
  Serial.println(F("  D6 : WS2811 DIN (Data In)"));
  Serial.println(F("         -> WS2811 Pin G: VORWAERTS  (Spitzenlicht)"));
  Serial.println(F("         -> WS2811 Pin R: RUECKWAERTS (Schlusslicht)"));
  Serial.println(F("--------------------------------------------------------"));
  Serial.println(F("A3144 Belegung (Beschriftete Seite zeigt zum Magneten!):"));
  Serial.println(F("  Pin 1 (links) : 5V VCC"));
  Serial.println(F("  Pin 2 (mitte) : GND"));
  Serial.println(F("  Pin 3 (rechts): OUT -> an D2 bzw. D3"));
  Serial.println(F("  WICHTIG: Sensor A und B in GETRENNTE Steckbrett-Spalten stecken!"));
  Serial.println(F("  WICHTIG: Neodym-Magnet mit SUEDPOL zur Beschriftung halten!"));
  Serial.println(F("--------------------------------------------------------"));
  Serial.println(F("WS2811 IC Belegung:"));
  Serial.println(F("  Pin 1 (OUTR): Rueckwaerts-Licht Rot (LED Kathode)"));
  Serial.println(F("  Pin 2 (OUTG): Vorwaerts-Licht Weiss (LED Kathode)"));
  Serial.println(F("  Pin 4 (GND) : Masse (Arduino GND)"));
  Serial.println(F("  Pin 6 (DIN) : Datenleitung an Arduino Pin D6"));
  Serial.println(F("  Pin 8 (VDD) : 5V Versorgungsspannung"));
  Serial.println(F("--------------------------------------------------------"));
  Serial.println(F("Befehle im Serial Monitor:"));
  Serial.println(F("  'h' / '?' : Dieses Menue anzeigen"));
  Serial.println(F("  's'       : Aktuellen Sensor- & WS2811-Status ausgeben"));
  Serial.println(F("  'g' / 'w' : Manuell VORWAERTS schalten (Kanal G)"));
  Serial.println(F("  'r'       : Manuell RUECKWAERTS schalten (Kanal R)"));
  Serial.println(F("  '+' / '-' : Helligkeit des aktiven Kanals erhoehen/senken"));
  Serial.println(F("  '0'..'9'  : Helligkeit des aktiven Kanals in Stufen setzen"));
  Serial.println(F("  't' / 'a' : Automatische Wechselschaltung (2s Takt) an/aus"));
  Serial.println(F("  'c'       : Zaehler zuruecksetzen"));
  Serial.println(F("========================================================\n"));
}

void printCurrentStatus() {
  int valA = digitalRead(PIN_HALL_A);
  int valB = digitalRead(PIN_HALL_B);

  Serial.println(F("\n--- AKTUELLER STATUS ---"));
  Serial.print(F("Pin D2 (Sensor A): "));
  Serial.print(valA == LOW ? F("AKTIV (LOW)") : F("RUHE (HIGH)"));
  Serial.print(F(" | Trigger gesamt: "));
  Serial.println(triggerCountA);
  Serial.print(F("Pin D3 (Sensor B): "));
  Serial.print(valB == LOW ? F("AKTIV (LOW)") : F("RUHE (HIGH)"));
  Serial.print(F(" | Trigger gesamt: "));
  Serial.println(triggerCountB);
  Serial.print(F("Gespeicherte Fahrtrichtung: "));
  printDirection(currentDirection);
  Serial.println();
  Serial.print(F("Automatische Wechselschaltung: "));
  Serial.println(autoToggleEnabled ? F("AKTIV (2s Takt)") : F("INAKTIV"));
  Serial.print(F("WS2811 an Pin D6: Kanal G (Vorwaerts)="));
  Serial.print(brightnessG);
  Serial.print(F(" | Kanal R (Rueckwaerts)="));
  Serial.print(brightnessR);
  Serial.print(F(" | Aktiv: "));
  if (currentDirection == DIR_FORWARD) {
    Serial.println(F("Kanal G (AN), Kanal R (AUS)"));
  } else if (currentDirection == DIR_REVERSE) {
    Serial.println(F("Kanal G (AUS), Kanal R (AN)"));
  } else {
    Serial.println(F("AUS"));
  }
  Serial.println(F("------------------------"));
}

void handleSerialCommands() {
  if (!Serial.available())
    return;
  char c = Serial.read();

  switch (c) {
  case 'h':
  case '?':
    printBanner();
    break;
  case 's':
    printCurrentStatus();
    break;
  case 'g':
  case 'w':
    autoToggleEnabled = false;
    currentDirection = DIR_FORWARD;
    applyLighting();
    Serial.println(F("-> Manuell gesetzt: VORWAERTS (WS2811 Kanal G)"));
    break;
  case 'r':
    autoToggleEnabled = false;
    currentDirection = DIR_REVERSE;
    applyLighting();
    Serial.println(F("-> Manuell gesetzt: RUECKWAERTS (WS2811 Kanal R)"));
    break;
  case 't':
  case 'a':
    autoToggleEnabled = !autoToggleEnabled;
    Serial.print(F("-> Automatische Wechselschaltung: "));
    Serial.println(autoToggleEnabled ? F("AKTIVIERT (2s Takt)") : F("PAUSIERT"));
    break;
  case '+':
    if (currentDirection == DIR_FORWARD) {
      brightnessG = (brightnessG <= 235) ? (brightnessG + 20) : 255;
      Serial.print(F("-> Helligkeit Kanal G (Vorwaerts) erhoeht auf: "));
      Serial.println(brightnessG);
    } else {
      brightnessR = (brightnessR <= 235) ? (brightnessR + 20) : 255;
      Serial.print(F("-> Helligkeit Kanal R (Rueckwaerts) erhoeht auf: "));
      Serial.println(brightnessR);
    }
    applyLighting();
    break;
  case '-':
    if (currentDirection == DIR_FORWARD) {
      brightnessG = (brightnessG >= 20) ? (brightnessG - 20) : 0;
      Serial.print(F("-> Helligkeit Kanal G (Vorwaerts) gesenkt auf: "));
      Serial.println(brightnessG);
    } else {
      brightnessR = (brightnessR >= 20) ? (brightnessR - 20) : 0;
      Serial.print(F("-> Helligkeit Kanal R (Rueckwaerts) gesenkt auf: "));
      Serial.println(brightnessR);
    }
    applyLighting();
    break;
  case '0':
  case '1':
  case '2':
  case '3':
  case '4':
  case '5':
  case '6':
  case '7':
  case '8':
  case '9': {
    uint8_t step = c - '0';
    uint8_t val = (uint8_t)(step * 28.33); // 0 .. 255
    if (currentDirection == DIR_FORWARD) {
      brightnessG = val;
      Serial.print(F("-> Helligkeit Kanal G (Vorwaerts) auf Stufe "));
    } else {
      brightnessR = val;
      Serial.print(F("-> Helligkeit Kanal R (Rueckwaerts) auf Stufe "));
    }
    Serial.print(step);
    Serial.print(F("/9 gesetzt (Wert: "));
    Serial.print(val);
    Serial.println(F(")"));
    applyLighting();
    break;
  }
  case 'c':
    triggerCountA = 0;
    triggerCountB = 0;
    Serial.println(F("-> Trigger-Zaehler auf 0 zurueckgesetzt."));
    break;
  case '\r':
  case '\n':
    // Leerzeichen ignorieren
    break;
  default:
    Serial.print(F("Unbekannter Befehl: '"));
    Serial.print(c);
    Serial.println(F("'. '?' fuer Hilfe."));
    break;
  }
}

// =====================================================================================
// SETUP & LOOP
// =====================================================================================

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 2500) {
    // Auf serielle Verbindung warten
  }

  // Pins konfigurieren
  pinMode(PIN_HALL_A, INPUT_PULLUP);
  pinMode(PIN_HALL_B, INPUT_PULLUP);
  pinMode(LED_BUILTIN, OUTPUT);

  // WS2811 initialisieren
  ws2811.begin();
  ws2811.show(); // Alle Kanäle zunächst dunkel

  // Initialen Lichtzustand herstellen (Startet mit Vorwärts/G)
  applyLighting();

  // Eventuell anstehende Interrupt-Flags vor dem Scharfschalten löschen
  EIFR = bit(INTF0) | bit(INTF1);

  // Hardware-Interrupts registrieren
  attachInterrupt(digitalPinToInterrupt(PIN_HALL_A), isr_hall_a, FALLING);
  attachInterrupt(digitalPinToInterrupt(PIN_HALL_B), isr_hall_b, FALLING);

  lastToggleTime = millis();
  printBanner();
  printCurrentStatus();
}

void loop() {
  // 1. Serielle Befehle einlesen
  handleSerialCommands();

  // 2. Auswertung neuer Interrupt-Ereignisse (Magnet)
  if (newTriggerEvent) {
    newTriggerEvent = false;

    // Bei Magnetimpuls automatischen Wechsel pausieren
    autoToggleEnabled = false;

    // Beleuchtung sofort aktualisieren
    applyLighting();

    // Detaillierten Diagnosebericht ausgeben
    Serial.print(F("[TRIGGER] "));
    printDirection(currentDirection);
    Serial.print(F(" | Methode 1 (Pegel B): "));
    printDirection(detectedByMethod1);
    Serial.print(F(" | Methode 2 (Sequenz): "));
    printDirection(detectedByMethod2);
    if (eventDeltaMs > 0) {
      Serial.print(F(" | Delta: "));
      Serial.print(eventDeltaMs);
      Serial.print(F(" ms"));
    }
    Serial.print(F(" | Zaehler A: "));
    Serial.print(triggerCountA);
    Serial.print(F(" B: "));
    Serial.println(triggerCountB);
  }

  // 3. Automatische Wechselschaltung (2 Sek Vorwärts/G, dann 2 Sek Rückwärts/R...)
  if (autoToggleEnabled) {
    unsigned long now = millis();
    if (now - lastToggleTime >= AUTO_TOGGLE_INTERVAL_MS) {
      lastToggleTime = now;
      if (currentDirection == DIR_FORWARD) {
        currentDirection = DIR_REVERSE;
      } else {
        currentDirection = DIR_FORWARD;
      }
      applyLighting();
      Serial.print(F("[AUTO-WECHSEL] "));
      printDirection(currentDirection);
      Serial.println(F(" (fuer 2 Sekunden)"));
    }
  }

  // 4. Kontinuierliche Pegelüberwachung (hilft bei statischem Heranhalten des Magneten)
  int currentPinA = digitalRead(PIN_HALL_A);
  int currentPinB = digitalRead(PIN_HALL_B);

  if (currentPinA != lastPinA || currentPinB != lastPinB) {
    lastPinA = currentPinA;
    lastPinB = currentPinB;

    // Wenn ein Sensor statisch LOW wird (z. B. Magnet steht direkt davor)
    if (currentPinA == LOW || currentPinB == LOW) {
      Serial.print(F("  -> Sensor-Pegel geaendert: A="));
      Serial.print(currentPinA == LOW ? F("MAGNET ERKANNT (LOW)")
                                      : F("OFFEN (HIGH)"));
      Serial.print(F(" | B="));
      Serial.println(currentPinB == LOW ? F("MAGNET ERKANNT (LOW)")
                                        : F("OFFEN (HIGH)"));
    }
  }

  delay(10);
}
