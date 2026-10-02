/* =====================================================================================
 * Projekt:     Autonomer Fahrtrichtungs-Lichtwechsel für H0-Steuerwagen (Märklin 78479)
 * Datei:       ws2811_test.ino
 * Beschreibung:Dediziertes Test- und Diagnoseprogramm für den WS2811 LED-Treiber.
 *              Vollkommen unabhängig von den Hall-Sensoren (keine Interrupts/Sensoren aktiv).
 *
 * Hardware-Anschluss:
 *   Arduino Pin D6   ---> WS2811 Pin 6 (DIN / Datenleitung)
 *   Arduino 5V       ---> WS2811 Pin 8 (VDD)
 *   Arduino GND      ---> WS2811 Pin 4 (GND)
 *   WS2811 Pin 1 (R) ---> Kathode (-) der Rückwärts-LEDs (Schlusslicht Rot)
 *   WS2811 Pin 2 (G) ---> Kathode (-) der Vorwärts-LEDs (Spitzenlicht Weiß)
 *   WS2811 Pin 3 (B) ---> Kathode (-) optional / unbeschaltet
 *   LED-Anoden (+)   ---> an +5V
 *
 * Baudrate:    115200 Baud
 * =====================================================================================
 */

#include <Arduino.h>
#include <Adafruit_NeoPixel.h>

// --- Pin- & WS2811-Konfiguration ---
const uint8_t PIN_WS2811 = 6;  // Datenleitung zum WS2811
const uint8_t NUM_PIXELS = 1;  // 1x WS2811 IC

// Standard: 800 kHz, Datenformat RGB (Byte 0 = OUTR, Byte 1 = OUTG, Byte 2 = OUTB)
#define WS2811_TYPE (NEO_RGB + NEO_KHZ800)
Adafruit_NeoPixel ws2811(NUM_PIXELS, PIN_WS2811, WS2811_TYPE);

// --- Test-Modi ---
enum TestMode {
  MODE_MANUAL = 0,    // Manuelle Steuerung über Tastaturbefehle
  MODE_AUTO_TOGGLE,   // Automatisches Umschalten (alle 2s: Vorwärts/G <-> Rückwärts/R)
  MODE_SOFT_FADE      // Kontinuierliches weiches Überblenden zwischen Vorwärts und Rückwärts
};

TestMode currentMode = MODE_AUTO_TOGGLE;

// Aktuelle Helligkeitsstufen (0 .. 255)
uint8_t brightnessLevel = 255;
uint8_t currentR = 0;
uint8_t currentG = 0;
uint8_t currentB = 0;

// Timing für automatische Modi
unsigned long lastActionTime = 0;
bool toggleState = false; // false = Vorwärts (G), true = Rückwärts (R)

// Fading-Variablen
int fadeDirection = 1;    // 1 = zu R hin faden, -1 = zu G hin faden
int fadeVal = 0;          // 0 = 100% G / 0% R; 255 = 0% G / 100% R
const uint16_t FADE_STEP_DELAY_MS = 15;

// =====================================================================================
// HILFSFUNKTIONEN
// =====================================================================================

void sendWS2811(uint8_t r, uint8_t g, uint8_t b) {
  currentR = r;
  currentG = g;
  currentB = b;
  ws2811.setPixelColor(0, r, g, b);
  ws2811.show();

  // Onboard-LED (Pin 13) als Indikator: leuchtet bei VORWAERTS (Kanal G aktiv)
  digitalWrite(LED_BUILTIN, (g > 0) ? HIGH : LOW);
}

void setForward() {
  sendWS2811(0, brightnessLevel, 0); // Kanal G an, R aus
}

void setReverse() {
  sendWS2811(brightnessLevel, 0, 0); // Kanal R an, G aus
}

void setChannelB(uint8_t val) {
  sendWS2811(0, 0, val);
}

void setAllChannels(uint8_t val) {
  sendWS2811(val, val, val);
}

void turnOff() {
  sendWS2811(0, 0, 0);
}

void printBanner() {
  Serial.println(F("\n========================================================"));
  Serial.println(F("       WS2811 STANDALONE HARDWARE-TEST (Pin D6)         "));
  Serial.println(F("========================================================"));
  Serial.println(F("Anschlussübersicht:"));
  Serial.println(F("  Arduino D6  ---> WS2811 Pin 6 (DIN)"));
  Serial.println(F("  Arduino 5V  ---> WS2811 Pin 8 (VDD)"));
  Serial.println(F("  Arduino GND ---> WS2811 Pin 4 (GND)"));
  Serial.println(F("  WS2811 G    ---> Kathode VORWAERTS-LED  (Pin 2 OUTG)"));
  Serial.println(F("  WS2811 R    ---> Kathode RUECKWAERTS-LED (Pin 1 OUTR)"));
  Serial.println(F("  LED Anoden  ---> an +5V"));
  Serial.println(F("--------------------------------------------------------"));
  Serial.println(F("Verfügbare Befehle:"));
  Serial.println(F("  'g' / '1' / 'w' : VORWAERTS (Kanal G aktiv)"));
  Serial.println(F("  'r' / '2'       : RUECKWAERTS (Kanal R aktiv)"));
  Serial.println(F("  'b' / '3'       : Nur Kanal B einschalten (Test dritter Pin)"));
  Serial.println(F("  '4'             : Beide Kanäle (G + R) gleichzeitig an"));
  Serial.println(F("  '0' / 'x'       : Alle Ausgänge AUS"));
  Serial.println(F("--------------------------------------------------------"));
  Serial.println(F("Automatische Modi:"));
  Serial.println(F("  'a'             : Automatischer Wechsel (2s Vorwärts <-> 2s Rückwärts)"));
  Serial.println(F("  'f'             : Sanfter Fading-Test (kontinuierliches Überblenden)"));
  Serial.println(F("--------------------------------------------------------"));
  Serial.println(F("Helligkeit & Diagnose:"));
  Serial.println(F("  '+' / '-'       : Helligkeit in 25er-Schritten anpassen"));
  Serial.println(F("  's'             : Aktuellen Status anzeigen"));
  Serial.println(F("  'h' / '?'       : Dieses Hilfemenü anzeigen"));
  Serial.println(F("========================================================\n"));
}

void printStatus() {
  Serial.println(F("\n--- AKTUELLER WS2811 STATUS ---"));
  Serial.print(F("Modus: "));
  switch (currentMode) {
    case MODE_MANUAL:      Serial.println(F("MANUELL")); break;
    case MODE_AUTO_TOGGLE: Serial.println(F("AUTOMATISCHER WECHSEL (2s Takt)")); break;
    case MODE_SOFT_FADE:   Serial.println(F("SANFTES UEBERBLENDEN (Fading)")); break;
  }
  Serial.print(F("Aktuelle Ausgabewerte -> G (Vorwaerts): "));
  Serial.print(currentG);
  Serial.print(F(" | R (Rueckwaerts): "));
  Serial.print(currentR);
  Serial.print(F(" | B: "));
  Serial.println(currentB);
  Serial.print(F("Helligkeits-Pegel: "));
  Serial.print(brightnessLevel);
  Serial.print(F(" ("));
  Serial.print((int)((brightnessLevel / 255.0) * 100));
  Serial.println(F("%)"));
  Serial.println(F("-------------------------------\n"));
}

void handleSerialInput() {
  if (!Serial.available()) return;
  char c = Serial.read();

  switch (c) {
    case 'h':
    case '?':
      printBanner();
      break;

    case 's':
      printStatus();
      break;

    case 'g':
    case '1':
    case 'w':
      currentMode = MODE_MANUAL;
      setForward();
      Serial.print(F("-> Manuell: VORWAERTS [Kanal G aktiv] (Wert: "));
      Serial.print(brightnessLevel);
      Serial.println(F(")"));
      break;

    case 'r':
    case '2':
      currentMode = MODE_MANUAL;
      setReverse();
      Serial.print(F("-> Manuell: RUECKWAERTS [Kanal R aktiv] (Wert: "));
      Serial.print(brightnessLevel);
      Serial.println(F(")"));
      break;

    case 'b':
    case '3':
      currentMode = MODE_MANUAL;
      setChannelB(brightnessLevel);
      Serial.print(F("-> Manuell: KANAL B aktiv (Wert: "));
      Serial.print(brightnessLevel);
      Serial.println(F(")"));
      break;

    case '4':
      currentMode = MODE_MANUAL;
      sendWS2811(brightnessLevel, brightnessLevel, 0);
      Serial.print(F("-> Manuell: VORWAERTS & RUECKWAERTS (G & R) aktiv (Wert: "));
      Serial.print(brightnessLevel);
      Serial.println(F(")"));
      break;

    case '0':
    case 'x':
      currentMode = MODE_MANUAL;
      turnOff();
      Serial.println(F("-> Manuell: ALLE KANAELE AUS"));
      break;

    case 'a':
      currentMode = MODE_AUTO_TOGGLE;
      toggleState = false;
      lastActionTime = millis();
      setForward();
      Serial.println(F("-> Modus: AUTOMATISCHER WECHSEL (2s Vorwärts/G <-> 2s Rückwärts/R) gestartet"));
      break;

    case 'f':
      currentMode = MODE_SOFT_FADE;
      fadeVal = 0;
      fadeDirection = 1;
      lastActionTime = millis();
      Serial.println(F("-> Modus: SANFTES UEBERBLENDEN gestartet"));
      break;

    case '+':
      brightnessLevel = (brightnessLevel <= 230) ? (brightnessLevel + 25) : 255;
      Serial.print(F("-> Helligkeit erhoeht auf: "));
      Serial.println(brightnessLevel);
      if (currentMode == MODE_MANUAL) {
        if (currentG > 0 && currentR == 0) setForward();
        else if (currentR > 0 && currentG == 0) setReverse();
        else if (currentB > 0) setChannelB(brightnessLevel);
        else if (currentR > 0 && currentG > 0) sendWS2811(brightnessLevel, brightnessLevel, 0);
      }
      break;

    case '-':
      brightnessLevel = (brightnessLevel >= 25) ? (brightnessLevel - 25) : 10;
      Serial.print(F("-> Helligkeit gesenkt auf: "));
      Serial.println(brightnessLevel);
      if (currentMode == MODE_MANUAL) {
        if (currentG > 0 && currentR == 0) setForward();
        else if (currentR > 0 && currentG == 0) setReverse();
        else if (currentB > 0) setChannelB(brightnessLevel);
        else if (currentR > 0 && currentG > 0) sendWS2811(brightnessLevel, brightnessLevel, 0);
      }
      break;

    case '\r':
    case '\n':
    case ' ':
      // Ignorieren
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
    // Auf USB-Seriell-Verbindung warten
  }

  pinMode(LED_BUILTIN, OUTPUT);

  // WS2811 initialisieren
  ws2811.begin();
  ws2811.show(); // Zunächst alle Ausgänge dunkel

  // Startzustand: Startet im automatischen Wechselmodus mit Vorwärts (Kanal G)
  setForward();
  lastActionTime = millis();

  printBanner();
  printStatus();
}

void loop() {
  // 1. Serielle Befehle einlesen
  handleSerialInput();

  // 2. Automatischer Wechselmodus (2 Sekunden Vorwärts/G, 2 Sekunden Rückwärts/R)
  if (currentMode == MODE_AUTO_TOGGLE) {
    unsigned long now = millis();
    if (now - lastActionTime >= 2000) {
      lastActionTime = now;
      toggleState = !toggleState;

      if (!toggleState) {
        setForward();
        Serial.println(F("[AUTO] -> VORWAERTS [Kanal G aktiv]"));
      } else {
        setReverse();
        Serial.println(F("[AUTO] -> RUECKWAERTS [Kanal R aktiv]"));
      }
    }
  }

  // 3. Sanfter Fading-Modus
  else if (currentMode == MODE_SOFT_FADE) {
    unsigned long now = millis();
    if (now - lastActionTime >= FADE_STEP_DELAY_MS) {
      lastActionTime = now;

      fadeVal += (fadeDirection * 4);

      if (fadeVal >= 255) {
        fadeVal = 255;
        fadeDirection = -1; // Richtung umkehren -> wieder zu Vorwärts (G)
      } else if (fadeVal <= 0) {
        fadeVal = 0;
        fadeDirection = 1;  // Richtung umkehren -> zu Rückwärts (R)
      }

      // fadeVal = 0 -> 100% G, 0% R
      // fadeVal = 255 -> 0% G, 100% R
      uint8_t valG = (uint16_t)((255 - fadeVal) * brightnessLevel) / 255;
      uint8_t valR = (uint16_t)(fadeVal * brightnessLevel) / 255;

      sendWS2811(valR, valG, 0);
    }
  }

  delay(5);
}
