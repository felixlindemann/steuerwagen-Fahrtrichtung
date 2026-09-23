/* =====================================================================================
 * Projekt:     Autonomer Fahrtrichtungs-Lichtwechsel für H0-Steuerwagen
 * (Märklin 78479) Datei:       breadboard_test.ino Beschreibung:Interaktiver
 * Diagnose- & Test-Sketch für den Breadboard-Aufbau.
 *              - Überprüfung der beiden A3144 Hall-Sensoren (Pinbelegung,
 * Funktion)
 *              - Testen der Magnetpolung (Reaktion auf Südpol an der
 * Beschriftungsseite)
 *              - Echtzeit-Auswertung beider Richtungserkennungs-Methoden:
 *                  * Methode 1: Klassische Quadratur (A fallend -> B
 * Pegelabfrage)
 *                  * Methode 2: Sequenzerkennung (A vor B vs. B vor A)
 *              - Ansteuerung von D4 (Weiß) und D5 (Rot PWM)
 *              - Serielle Befehle zur Helligkeitsjustierung und Simulation
 * Baudrate:    115200 Baud
 * =====================================================================================
 */

#include <Arduino.h>

// --- Pin-Definitionen ---
const uint8_t PIN_HALL_A = 2; // Hardware-Interrupt INT0
const uint8_t PIN_HALL_B = 3; // Hardware-Interrupt INT1
const uint8_t PIN_LED_WHITE =
    4; // Frontlicht (3x warmweiß parallel mit je 1 kΩ)
const uint8_t PIN_LED_RED =
    5; // Schlusslicht (2x rot parallel mit je 1 kΩ, PWM)

// --- Richtungs-Definitionen ---
enum Direction {
  DIR_UNKNOWN = 0,
  DIR_FORWARD, // Vorwärts: Frontlicht weiß aktiv
  DIR_REVERSE  // Rückwärts: Schlusslicht rot aktiv
};

// --- Globale Variablen für Interrupts & Sensorik ---
volatile Direction currentDirection = DIR_FORWARD;
volatile unsigned long lastTriggerTimeA = 0;
volatile unsigned long lastTriggerTimeB = 0;
volatile uint32_t triggerCountA = 0;
volatile uint32_t triggerCountB = 0;
volatile bool newTriggerEvent = false;
volatile Direction detectedByMethod1 = DIR_UNKNOWN; // Quadratur-Pegel
volatile Direction detectedByMethod2 = DIR_UNKNOWN; // Sequenz-Reihenfolge
volatile unsigned long eventDeltaMs = 0;

// Entprell-Zeitfenster in Millisekunden (bei H0-Höchstgeschwindigkeit ca. 8 ms
// Mindestzeit)
const unsigned long DEBOUNCE_MS = 8;
// Maximales Zeitfenster (ms) zwischen Sensor A und B für eine zusammenhängende
// Drehung
const unsigned long SEQUENCE_TIMEOUT_MS = 2500;

// PWM-Helligkeit für D5 (Rot) im Bereich 0 .. 255
uint8_t redPwmValue = 180;

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

  // Methode 1: Klassische Quadratur-Pegelabfrage
  // Wenn A schaltet (fallende Flanke) und B ist noch HIGH -> A löste vor B aus
  // -> Vorwärts Wenn A schaltet und B ist bereits aktiv (LOW) -> B löste vor A
  // aus -> Rückwärts
  if (stateB == HIGH) {
    detectedByMethod1 = DIR_FORWARD;
  } else {
    detectedByMethod1 = DIR_REVERSE;
  }

  // Methode 2: Sequenzzeitpunkt-Vergleich
  if (lastTriggerTimeB > 0 && (now - lastTriggerTimeB) < SEQUENCE_TIMEOUT_MS) {
    // Sensor B hat kurz zuvor ausgelöst -> Bewegung war B -> A = Rückwärts
    detectedByMethod2 = DIR_REVERSE;
    eventDeltaMs = now - lastTriggerTimeB;
  } else {
    detectedByMethod2 = DIR_FORWARD;
    eventDeltaMs = 0;
  }

  // Priorisierung: Wenn Methode 1 eindeutig (B==LOW) war B sicher aktiv.
  // Wenn B==HIGH war, bestätigt Methode 2, ob A zuerst kam.
  currentDirection =
      (detectedByMethod1 == DIR_REVERSE) ? DIR_REVERSE : detectedByMethod2;

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

  // Methode 2: Wenn Sensor A kurz zuvor ausgelöst hat -> A -> B = Vorwärts
  if (lastTriggerTimeA > 0 && (now - lastTriggerTimeA) < SEQUENCE_TIMEOUT_MS) {
    detectedByMethod2 = DIR_FORWARD;
    eventDeltaMs = now - lastTriggerTimeA;
    currentDirection = DIR_FORWARD;
  } else {
    // B hat als erstes ausgelöst -> bereite Rückwärts vor
    detectedByMethod2 = DIR_REVERSE;
    eventDeltaMs = 0;
  }

  lastTriggerTimeB = now;
  triggerCountB++;
  newTriggerEvent = true;
}

// Polarität & Fahrtrichtung:
// true = Im Standard leuchtet Weiß (Active-LOW / Gemeinsame Anode)
const bool LED_ACTIVE_LOW = true;

// =====================================================================================
// HILFSFUNKTIONEN
// =====================================================================================

void setWhiteLed(bool on) {
  if (LED_ACTIVE_LOW) {
    digitalWrite(PIN_LED_WHITE, on ? LOW : HIGH);
  } else {
    digitalWrite(PIN_LED_WHITE, on ? HIGH : LOW);
  }
}

void setRedLed(uint8_t brightness) {
  if (LED_ACTIVE_LOW) {
    analogWrite(PIN_LED_RED, 255 - brightness);
  } else {
    analogWrite(PIN_LED_RED, brightness);
  }
}

void applyLighting() {
  if (currentDirection == DIR_FORWARD) {
    setWhiteLed(true);
    setRedLed(0); // Rot aus
    digitalWrite(LED_BUILTIN, HIGH);
  } else if (currentDirection == DIR_REVERSE) {
    setWhiteLed(false); // Weiß aus
    setRedLed(redPwmValue); // Rot gedimmt
    digitalWrite(LED_BUILTIN, LOW);
  }
}

void printDirection(Direction d) {
  switch (d) {
  case DIR_FORWARD:
    Serial.print(F("VORWAERTS [Front Weiss]"));
    break;
  case DIR_REVERSE:
    Serial.print(F("RUECKWAERTS [Schluss Rot]"));
    break;
  default:
    Serial.print(F("UNBEKANNT"));
    break;
  }
}

void printBanner() {
  Serial.println(
      F("\n========================================================"));
  Serial.println(F("  H0-STEUERWAGEN (Märklin 78479) - BREADBOARD TEST"));
  Serial.println(F("  Autonomer Fahrtrichtungs-Lichtwechsel"));
  Serial.println(F("========================================================"));
  Serial.println(F("Pin-Belegung Arduino Nano:"));
  Serial.println(F("  D2 : Hall-Sensor A (INT0, INPUT_PULLUP)"));
  Serial.println(F("  D3 : Hall-Sensor B (INT1, INPUT_PULLUP)"));
  Serial.println(F("  D4 : Frontlicht Weiss (3x LED via Vorwiderstand)"));
  Serial.println(F("  D5 : Schlusslicht Rot (2x LED via PWM)"));
  Serial.println(F("--------------------------------------------------------"));
  Serial.println(F("A3144 Belegung (Beschriftete Seite zeigt zum Magneten!):"));
  Serial.println(F("  Pin 1 (links) : 5V VCC"));
  Serial.println(F("  Pin 2 (mitte) : GND"));
  Serial.println(F("  Pin 3 (rechts): OUT -> an D2 bzw. D3"));
  Serial.println(
      F("  WICHTIG: Sensor A und B in GETRENNTE Steckbrett-Spalten stecken!"));
  Serial.println(
      F("  WICHTIG: Neodym-Magnet mit SUEDPOL zur Beschriftung halten!"));
  Serial.println(F("--------------------------------------------------------"));
  Serial.println(F("Befehle im Serial Monitor:"));
  Serial.println(F("  'h' / '?' : Dieses Menue anzeigen"));
  Serial.println(F("  's'       : Aktuellen Sensor- & Licht-Status ausgeben"));
  Serial.println(F("  'w'       : Manuell Frontlicht WEISS einschalten"));
  Serial.println(F("  'r'       : Manuell Schlusslicht ROT einschalten"));
  Serial.println(F("  '+' / '-' : PWM-Helligkeit fuer ROT erhoehen/senken"));
  Serial.println(
      F("  '0'..'9'  : Helligkeit Rot direkt in 10er-Stufen setzen"));
  Serial.println(F("  'c'       : Zaehler zuruecksetzen"));
  Serial.println(
      F("========================================================\n"));
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
  Serial.print(F("LED-Ausgaenge: D4 (Weiss)="));
  Serial.print(digitalRead(PIN_LED_WHITE));
  Serial.print(F(" | D5 (Rot PWM)="));
  Serial.print(redPwmValue);
  Serial.println(F("\n------------------------"));
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
  case 'w':
    currentDirection = DIR_FORWARD;
    applyLighting();
    Serial.println(F("-> Manuell gesetzt: VORWAERTS (Frontlicht Weiss)"));
    break;
  case 'r':
    currentDirection = DIR_REVERSE;
    applyLighting();
    Serial.println(F("-> Manuell gesetzt: RUECKWAERTS (Schlusslicht Rot)"));
    break;
  case '+':
    if (redPwmValue <= 235)
      redPwmValue += 20;
    else
      redPwmValue = 255;
    applyLighting();
    Serial.print(F("-> Rotes PWM erhoeht auf: "));
    Serial.println(redPwmValue);
    break;
  case '-':
    if (redPwmValue >= 20)
      redPwmValue -= 20;
    else
      redPwmValue = 0;
    applyLighting();
    Serial.print(F("-> Rotes PWM gesenkt auf: "));
    Serial.println(redPwmValue);
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
    redPwmValue = (uint8_t)(step * 28.33); // 0 .. 255
    applyLighting();
    Serial.print(F("-> Rotes PWM auf Stufe "));
    Serial.print(step);
    Serial.print(F("/9 gesetzt (Wert: "));
    Serial.print(redPwmValue);
    Serial.println(F(")"));
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
  pinMode(PIN_LED_WHITE, OUTPUT);
  pinMode(PIN_LED_RED, OUTPUT);
  pinMode(LED_BUILTIN, OUTPUT);

  // Initialen Lichtzustand herstellen (Standard: Vorwärts = Frontlicht Weiß an)
  applyLighting();

  // Eventuell anstehende Interrupt-Flags vor dem Scharfschalten löschen
  EIFR = bit(INTF0) | bit(INTF1);

  // Hardware-Interrupts registrieren
  attachInterrupt(digitalPinToInterrupt(PIN_HALL_A), isr_hall_a, FALLING);
  attachInterrupt(digitalPinToInterrupt(PIN_HALL_B), isr_hall_b, FALLING);

  printBanner();
  printCurrentStatus();
}

void loop() {
  // 1. Serielle Befehle einlesen
  handleSerialCommands();

  // 2. Auswertung neuer Interrupt-Ereignisse
  if (newTriggerEvent) {
    newTriggerEvent = false;

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

  // 3. Kontinuierliche Pegelüberwachung (hilft bei statischem Heranhalten des
  // Magneten)
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
