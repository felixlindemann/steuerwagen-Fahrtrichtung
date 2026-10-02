/* =====================================================================================
 * Projekt:     Autonomer Fahrtrichtungs-Lichtwechsel für H0-Steuerwagen (Märklin 78479)
 * Datei:       steuerwagen_lichtwechsel.ino
 * Modell:      H0 Doppelstock-Steuerwagen DBbzfa 761 (Märklin 78479)
 * Controller:  Arduino Nano (ATmega328P, 5V / 16 MHz)
 *
 * Funktionsweise:
 * - 2x unipolarer Hall-Sensor A3144 (D2/INT0 & D3/INT1) erfassen Raddrehung
 * - Neodym-Magnet (2x1 mm, Südpol nach außen) auf innerer Achse des hinteren Drehgestells
 * - Ansteuerung eines WS2811 LED-Treibers an Pin D6:
 *     * Vorwärts:  Kanal G aktiv (Pin G des WS2811, z. B. Spitzenlicht weiß)
 *     * Rückwärts: Kanal R aktiv (Pin R des WS2811, z. B. Schlusslicht rot)
 * - Beibehaltung des Zustands im Stillstand
 * - Sanftes Überblenden (Soft-Fade) zwischen Kanal G und R
 * - Flacker- und Rauschunterdrückung gegen Gleisunregelmäßigkeiten
 * =====================================================================================
 */

#include <Arduino.h>
#include <Adafruit_NeoPixel.h>

// =====================================================================================
// KONFIGURATION & PIN-MAPPING
// =====================================================================================

// Pinbelegung
const uint8_t PIN_HALL_A = 2;    // Hall-Sensor A (INT0, Hardware-Interrupt)
const uint8_t PIN_HALL_B = 3;    // Hall-Sensor B (INT1, Hardware-Interrupt)
const uint8_t PIN_WS2811 = 6;    // WS2811 Datenleitung DIN (Steuert Kanal G & R)
const uint8_t NUM_PIXELS = 1;    // 1x WS2811 IC

// WS2811 Objekt initialisieren (Standard: 800 kHz, Datenformat RGB für WS2811-Pins OUTR, OUTG, OUTB)
#define WS2811_TYPE (NEO_RGB + NEO_KHZ800)
Adafruit_NeoPixel ws2811(NUM_PIXELS, PIN_WS2811, WS2811_TYPE);

// Beleuchtungs-Konfiguration
const uint8_t BRIGHTNESS_G = 255;   // 0 .. 255: Helligkeit für Kanal G (Vorwärts / Spitzenlicht)
const uint8_t BRIGHTNESS_R = 255;   // 0 .. 255: Helligkeit für Kanal R (Rückwärts / Schlusslicht)
const bool ENABLE_SOFT_FADE = true; // Sanfter Übergang (weiches Auf-/Abblenden)
const uint16_t FADE_STEP_MS = 12;   // Zeit pro Fading-Schritt (weicher Wechsel ~200-250ms)

// Entprellung & Sensorik-Timing
// Bei H0 Maßstab 1:87 entspricht Vorbild-Höchstgeschwindigkeit (160 km/h) ca. 0.51 m/s.
// Raddurchmesser ca. 10.4 mm -> Radumfang ~32.7 mm -> ca. 15.6 Umdrehungen pro Sekunde (~64 ms pro Umdrehung).
// Das gemessene Zeitfenster zwischen den eng benachbarten Sensoren B und A beträgt bei Handvorschub ca. 75-80 ms.
// Mit SEQUENCE_TIMEOUT_MS = 600 ms werden auch Kriechgeschwindigkeiten sicher erfasst.
const unsigned long DEBOUNCE_MS = 8;
const unsigned long SEQUENCE_TIMEOUT_MS = 600; // Max. Zeitabstand für Durchgang zwischen den Sensoren

// =====================================================================================
// TYPEN & ZUSTANDSVARIABLEN
// =====================================================================================

enum Direction : uint8_t {
  DIR_FORWARD = 0, // Kanal G an (Vorwärts), Kanal R aus
  DIR_REVERSE = 1  // Kanal R an (Rückwärts), Kanal G aus
};

// Volatile Variablen für ISR
volatile Direction targetDirection = DIR_FORWARD;
volatile bool directionChanged = false;
volatile unsigned long lastTriggerA = 0;
volatile unsigned long lastTriggerB = 0;

// Aktueller Ausgabe-Status (für Fading)
Direction activeDirection = DIR_FORWARD;
uint8_t currentG = 0;
uint8_t currentR = 0;
uint8_t targetG = BRIGHTNESS_G;
uint8_t targetR = 0;
unsigned long lastFadeTime = 0;

// =====================================================================================
// INTERRUPT SERVICE ROUTINES (ISRs)
// =====================================================================================

// ISR Sensor A (D2 / INT0) - Reagiert auf fallende Flanke
void isr_hall_a() {
  unsigned long now = millis();
  if (now - lastTriggerA < DEBOUNCE_MS) {
    return; // Prellen / Gleisstörung verwerfen
  }

  int stateB = digitalRead(PIN_HALL_B);

  // 1. Fall: Signale überlappen sich (Sensor B ist noch aktiv LOW)
  // B war zuerst da -> Bewegung B -> A = VORWÄRTS
  if (stateB == LOW) {
    if (targetDirection != DIR_FORWARD) {
      targetDirection = DIR_FORWARD;
      directionChanged = true;
    }
    lastTriggerA = 0;
    lastTriggerB = 0;
    return;
  }

  // 2. Fall: Sequenz B -> A innerhalb des Zeitfensters
  if (lastTriggerB > 0 && (now - lastTriggerB) < SEQUENCE_TIMEOUT_MS) {
    // Sequenz B -> A vollendet: Eindeutig VORWÄRTS
    if (targetDirection != DIR_FORWARD) {
      targetDirection = DIR_FORWARD;
      directionChanged = true;
    }
    // Sequenz abgeschlossen: Flags verbrauchen
    lastTriggerA = 0;
    lastTriggerB = 0;
    return;
  }

  // B war nicht aktiv und kam nicht zuvor -> A startet eventuell eine A -> B Sequenz
  lastTriggerA = now;
}

// ISR Sensor B (D3 / INT1) - Reagiert auf fallende Flanke
void isr_hall_b() {
  unsigned long now = millis();
  if (now - lastTriggerB < DEBOUNCE_MS) {
    return; // Prellen verwerfen
  }

  int stateA = digitalRead(PIN_HALL_A);

  // 1. Fall: Signale überlappen sich (Sensor A ist noch aktiv LOW)
  // A war zuerst da -> Bewegung A -> B = RÜCKWÄRTS
  if (stateA == LOW) {
    if (targetDirection != DIR_REVERSE) {
      targetDirection = DIR_REVERSE;
      directionChanged = true;
    }
    lastTriggerA = 0;
    lastTriggerB = 0;
    return;
  }

  // 2. Fall: Sequenz A -> B innerhalb des Zeitfensters
  if (lastTriggerA > 0 && (now - lastTriggerA) < SEQUENCE_TIMEOUT_MS) {
    // Sequenz A -> B vollendet: Eindeutig RÜCKWÄRTS
    if (targetDirection != DIR_REVERSE) {
      targetDirection = DIR_REVERSE;
      directionChanged = true;
    }
    // Sequenz abgeschlossen: Flags verbrauchen
    lastTriggerA = 0;
    lastTriggerB = 0;
    return;
  }

  // A war nicht aktiv und kam nicht zuvor -> B startet eventuell eine B -> A Sequenz
  lastTriggerB = now;
}

// =====================================================================================
// BELEUCHTUNGSSTEUERUNG (WS2811)
// =====================================================================================

void setWS2811(uint8_t r, uint8_t g, uint8_t b = 0) {
  ws2811.setPixelColor(0, r, g, b);
  ws2811.show();
}

void updateLightingInstant() {
  if (targetDirection == DIR_FORWARD) {
    currentG = BRIGHTNESS_G;
    currentR = 0;
  } else {
    currentG = 0;
    currentR = BRIGHTNESS_R;
  }
  setWS2811(currentR, currentG, 0);
  activeDirection = targetDirection;
}

void updateLightingFaded() {
  unsigned long now = millis();
  if (now - lastFadeTime < FADE_STEP_MS) {
    return;
  }
  lastFadeTime = now;

  // Zielwerte festlegen
  if (targetDirection == DIR_FORWARD) {
    targetG = BRIGHTNESS_G;
    targetR = 0;
  } else {
    targetG = 0;
    targetR = BRIGHTNESS_R;
  }

  bool changed = false;

  // Kanal G faden
  if (currentG < targetG) {
    currentG = min((uint16_t)targetG, (uint16_t)(currentG + 15));
    changed = true;
  } else if (currentG > targetG) {
    currentG = (currentG > 15) ? (currentG - 15) : 0;
    changed = true;
  }

  // Kanal R faden
  if (currentR < targetR) {
    currentR = min((uint16_t)targetR, (uint16_t)(currentR + 15));
    changed = true;
  } else if (currentR > targetR) {
    currentR = (currentR > 15) ? (currentR - 15) : 0;
    changed = true;
  }

  if (changed) {
    setWS2811(currentR, currentG, 0);
  }

  activeDirection = targetDirection;
}

// =====================================================================================
// ARDUINO SETUP & MAIN LOOP
// =====================================================================================

void setup() {
  // Serielle Schnittstelle für Debugging
  Serial.begin(115200);

  // Pins initialisieren
  pinMode(PIN_HALL_A, INPUT_PULLUP);
  pinMode(PIN_HALL_B, INPUT_PULLUP);
  pinMode(LED_BUILTIN, OUTPUT); // Onboard-LED auf Pin 13 als optischer Indikator

  // WS2811 initialisieren
  ws2811.begin();
  ws2811.show(); // Alle Kanäle zunächst dunkel

  // Initialer Zustand: Vorwärts (Kanal G an, Onboard-LED an)
  updateLightingInstant();
  digitalWrite(LED_BUILTIN, HIGH);

  // Hardware-Interrupts scharfschalten
  attachInterrupt(digitalPinToInterrupt(PIN_HALL_A), isr_hall_a, FALLING);
  attachInterrupt(digitalPinToInterrupt(PIN_HALL_B), isr_hall_b, FALLING);

  Serial.println(F("\n========================================================"));
  Serial.println(F(" Steuerwagen Fahrtrichtungs-Lichtwechsel (Märklin 78479)"));
  Serial.println(F(" WS2811 an Pin D6 (Kanal G = Vorwaerts, Kanal R = Rueckwaerts)"));
  Serial.println(F(" Debug-Ausgabe aktiv (115200 Baud)"));
  Serial.println(F(" Hinweis: Onboard-LED (Pin 13) leuchtet bei VORWAERTS"));
  Serial.println(F(" Initialer Zustand: VORWAERTS [WS2811 Kanal G aktiv]"));
  Serial.println(F("========================================================\n"));
}

void loop() {
  // Debug-Ausgabe & Statusmeldung bei erkanntem Richtungswechsel
  if (directionChanged) {
    directionChanged = false;

    Serial.print(F("["));
    Serial.print(millis());
    Serial.print(F(" ms] RICHTUNGSWECHSEL ERKANNT -> "));

    if (targetDirection == DIR_FORWARD) {
      Serial.println(F(">>> VORWAERTS (WS2811 Kanal G) | Onboard-LED: AN"));
      digitalWrite(LED_BUILTIN, HIGH);
    } else {
      Serial.println(F("<<< RUECKWAERTS (WS2811 Kanal R) | Onboard-LED: AUS"));
      digitalWrite(LED_BUILTIN, LOW);
    }

    if (!ENABLE_SOFT_FADE) {
      updateLightingInstant();
    }
  }

  // Sanftes Überblenden (falls aktiviert)
  if (ENABLE_SOFT_FADE) {
    updateLightingFaded();
  }
}
