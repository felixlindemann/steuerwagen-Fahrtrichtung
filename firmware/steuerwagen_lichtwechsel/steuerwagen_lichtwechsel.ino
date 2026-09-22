/* =====================================================================================
 * Projekt:     Autonomer Fahrtrichtungs-Lichtwechsel für H0-Steuerwagen (Märklin 78479)
 * Datei:       steuerwagen_lichtwechsel.ino
 * Modell:      H0 Doppelstock-Steuerwagen DBbzfa 761 (Märklin 78479)
 * Controller:  Arduino Nano (ATmega328P, 5V / 16 MHz)
 *
 * Funktionsweise:
 * - 2x unipolarer Hall-Sensor A3144 (D2/INT0 & D3/INT1) erfassen Raddrehung
 * - Neodym-Magnet (2x1 mm, Südpol nach außen) auf innerer Achse des hinteren Drehgestells
 * - Automatische Umschaltung zwischen:
 *     * Vorwärts (Wagen voraus / geschoben): 3x warmweißes Spitzensignal (Pin D4)
 *     * Rückwärts (Lok zieht / Steuerwagen hinten): 2x rotes Schlusslicht (Pin D5, PWM-gedimmt)
 * - Beibehaltung des Zustands im Stillstand
 * - Flacker- und Rauschunterdrückung gegen Gleisunregelmäßigkeiten
 * ===================================================================================== */

#include <Arduino.h>

// =====================================================================================
// KONFIGURATION & PIN-MAPPING
// =====================================================================================

// Pinbelegung
const uint8_t PIN_HALL_A    = 2;  // Hall-Sensor A (INT0, Hardware-Interrupt)
const uint8_t PIN_HALL_B    = 3;  // Hall-Sensor B (INT1, Hardware-Interrupt)
const uint8_t PIN_LED_WHITE = 4;  // 3x Front-LEDs Weiß (parallel mit je 1 kΩ)
const uint8_t PIN_LED_RED   = 5;  // 2x Schluss-LEDs Rot (parallel mit je 1 kΩ, Timer0 PWM)

// Beleuchtungs-Konfiguration
const uint8_t RED_PWM_BRIGHTNESS = 160;  // 0 .. 255: Dimmung des roten Schlusslichts
const bool    ENABLE_SOFT_FADE   = true; // Sanfter Übergang (weiches Auf-/Abblenden)
const uint16_t FADE_STEP_MS      = 12;   // Zeit pro Fading-Schritt (weicher Wechsel ~250ms)

// Entprellung & Sensorik-Timing
// Bei H0 Maßstab 1:87 entspricht Vorbild-Höchstgeschwindigkeit (160 km/h) ca. 0.51 m/s.
// Raddurchmesser ca. 10.4 mm -> Radumfang ~32.7 mm -> ca. 15.6 Umdrehungen pro Sekunde (~64 ms pro Umdrehung).
// Ein Entprellfenster von 8 ms schützt perfekt vor HF-Störungen ohne Impulse bei Maximaltempo zu verlieren.
const unsigned long DEBOUNCE_MS        = 8;
const unsigned long SEQUENCE_TIMEOUT_MS = 2500; // Max. Zeitabstand für Richtungserkennung

// =====================================================================================
// TYPEN & ZUSTANDSVARIABLEN
// =====================================================================================

enum Direction : uint8_t {
    DIR_FORWARD = 0, // Weiß an, Rot aus
    DIR_REVERSE = 1  // Weiß aus, Rot an (PWM)
};

// Volatile Variablen für ISR
volatile Direction targetDirection   = DIR_FORWARD;
volatile bool      directionChanged  = false;
volatile unsigned long lastTriggerA  = 0;
volatile unsigned long lastTriggerB  = 0;

// Aktueller Ausgabe-Status (für Fading)
Direction activeDirection = DIR_FORWARD;
uint8_t currentRedPwm   = 0;
uint8_t targetRedPwm    = 0;
bool    currentWhiteOn  = true;
bool    targetWhiteOn   = true;
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
    // Wenn B schon aktiv war, kam B vor A -> Eindeutig RÜCKWÄRTS
    if (stateB == LOW) {
        if (targetDirection != DIR_REVERSE) {
            targetDirection  = DIR_REVERSE;
            directionChanged = true;
        }
    } 
    // 2. Fall: Keine Überlappung (B ist HIGH)
    // Prüfen, ob B kurz zuvor ausgelöst hatte
    else if (lastTriggerB > 0 && (now - lastTriggerB) < SEQUENCE_TIMEOUT_MS) {
        // B feuerte kurz vor A -> RÜCKWÄRTS
        if (targetDirection != DIR_REVERSE) {
            targetDirection  = DIR_REVERSE;
            directionChanged = true;
        }
    } else {
        // A hat als erstes gefeuert -> VORWÄRTS
        if (targetDirection != DIR_FORWARD) {
            targetDirection  = DIR_FORWARD;
            directionChanged = true;
        }
    }

    lastTriggerA = now;
}

// ISR Sensor B (D3 / INT1) - Reagiert auf fallende Flanke
void isr_hall_b() {
    unsigned long now = millis();
    if (now - lastTriggerB < DEBOUNCE_MS) {
        return; // Prellen verwerfen
    }

    // Wenn Sensor A kurz zuvor gefeuert hat -> Sequenz A -> B = VORWÄRTS
    if (lastTriggerA > 0 && (now - lastTriggerA) < SEQUENCE_TIMEOUT_MS) {
        if (targetDirection != DIR_FORWARD) {
            targetDirection  = DIR_FORWARD;
            directionChanged = true;
        }
    }

    lastTriggerB = now;
}

// =====================================================================================
// BELEUCHTUNGSSTEUERUNG
// =====================================================================================

void updateLightingInstant() {
    if (targetDirection == DIR_FORWARD) {
        digitalWrite(PIN_LED_WHITE, HIGH);
        analogWrite(PIN_LED_RED, 0);
        currentWhiteOn = true;
        currentRedPwm  = 0;
    } else {
        digitalWrite(PIN_LED_WHITE, LOW);
        analogWrite(PIN_LED_RED, RED_PWM_BRIGHTNESS);
        currentWhiteOn = false;
        currentRedPwm  = RED_PWM_BRIGHTNESS;
    }
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
        targetWhiteOn = true;
        targetRedPwm  = 0;
    } else {
        targetWhiteOn = false;
        targetRedPwm  = RED_PWM_BRIGHTNESS;
    }

    // Rotes Licht faden
    if (currentRedPwm < targetRedPwm) {
        currentRedPwm = min((uint16_t)targetRedPwm, (uint16_t)(currentRedPwm + 15));
        analogWrite(PIN_LED_RED, currentRedPwm);
    } else if (currentRedPwm > targetRedPwm) {
        currentRedPwm = (currentRedPwm > 15) ? (currentRedPwm - 15) : 0;
        analogWrite(PIN_LED_RED, currentRedPwm);
    }

    // Weißes Licht umschalten (erst wenn Rot weitgehend ausgeblendet ist bzw. sanft geschaltet)
    if (targetWhiteOn && !currentWhiteOn && currentRedPwm < 50) {
        digitalWrite(PIN_LED_WHITE, HIGH);
        currentWhiteOn = true;
    } else if (!targetWhiteOn && currentWhiteOn) {
        digitalWrite(PIN_LED_WHITE, LOW);
        currentWhiteOn = false;
    }

    activeDirection = targetDirection;
}

// =====================================================================================
// ARDUINO SETUP & MAIN LOOP
// =====================================================================================

void setup() {
    // Serielle Schnittstelle für Debugging ohne LEDs
    Serial.begin(115200);

    // Pins initialisieren
    pinMode(PIN_HALL_A, INPUT_PULLUP);
    pinMode(PIN_HALL_B, INPUT_PULLUP);
    pinMode(PIN_LED_WHITE, OUTPUT);
    pinMode(PIN_LED_RED, OUTPUT);
    pinMode(LED_BUILTIN, OUTPUT); // Onboard-LED auf Pin 13 als optischer Indikator

    // Initialer Zustand: Vorwärts (Frontlicht weiß an, Onboard-LED an)
    updateLightingInstant();
    digitalWrite(LED_BUILTIN, HIGH);

    // Hardware-Interrupts scharfschalten
    attachInterrupt(digitalPinToInterrupt(PIN_HALL_A), isr_hall_a, FALLING);
    attachInterrupt(digitalPinToInterrupt(PIN_HALL_B), isr_hall_b, FALLING);

    Serial.println(F("\n========================================================"));
    Serial.println(F(" Steuerwagen Fahrtrichtungs-Lichtwechsel (Märklin 78479)"));
    Serial.println(F(" Debug-Ausgabe aktiv (115200 Baud)"));
    Serial.println(F(" Hinweis: Onboard-LED (Pin 13) leuchtet bei VORWAERTS"));
    Serial.println(F(" Initialer Zustand: VORWAERTS [Frontlicht WEISS]"));
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
            Serial.println(F(">>> VORWAERTS (Frontlicht WEISS) | Onboard-LED: AN"));
            digitalWrite(LED_BUILTIN, HIGH);
        } else {
            Serial.println(F("<<< RUECKWAERTS (Schlusslicht ROT) | Onboard-LED: AUS"));
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

