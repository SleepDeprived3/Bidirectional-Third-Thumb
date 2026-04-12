// ─────────────────────────────────────────────────────────────
// Haptic Motor Test — Adafruit Feather M0 BLE
// Circuit: BC547 NPN transistor + 1N4148 diode on GPIO D5
// ─────────────────────────────────────────────────────────────

#define MOTOR_PIN 5   // D5 — change if you wired to a different pin

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 3000);  // wait for Serial Monitor (max 3s)

  pinMode(MOTOR_PIN, OUTPUT);
  digitalWrite(MOTOR_PIN, LOW);        // ensure motor is off at boot

  Serial.println("─────────────────────────────────");
  Serial.println(" Haptic Motor Test — Feather M0");
  Serial.println("─────────────────────────────────");
  Serial.println("Commands via Serial Monitor:");
  Serial.println("  1 → Single short buzz");
  Serial.println("  2 → Three quick pulses");
  Serial.println("  3 → Long buzz (1 second)");
  Serial.println("  4 → Heartbeat pattern");
  Serial.println("  5 → Ramp up & down (PWM)");
  Serial.println("  s → Stop motor immediately");
  Serial.println("─────────────────────────────────");
}

void loop() {
  // Run a startup confirmation buzz once on first loop
  static bool startupDone = false;
  if (!startupDone) {
    startupDone = true;
    Serial.println("Running startup buzz...");
    singleBuzz(200);
    delay(200);
    singleBuzz(200);
    Serial.println("Ready. Send a command.");
  }

  // Listen for Serial commands
  if (Serial.available()) {
    char cmd = Serial.read();

    switch (cmd) {
      case '1':
        Serial.println("► Single short buzz");
        singleBuzz(150);
        break;

      case '2':
        Serial.println("► Three quick pulses");
        for (int i = 0; i < 3; i++) {
          singleBuzz(80);
          delay(80);
        }
        break;

      case '3':
        Serial.println("► Long buzz (1 second)");
        singleBuzz(1000);
        break;

      case '4':
        Serial.println("► Heartbeat pattern");
        heartbeat();
        break;

      case '5':
        Serial.println("► Ramp up and down (PWM)");
        rampUpDown();
        break;

      case 's':
      case 'S':
        digitalWrite(MOTOR_PIN, LOW);
        Serial.println("■ Motor stopped");
        break;

      case '\n':
      case '\r':
        break;  // ignore newline characters

      default:
        Serial.print("Unknown command: ");
        Serial.println(cmd);
        break;
    }
  }
}

// ── Helpers ──────────────────────────────────────────────────

// Turn motor on for a set duration, then off
void singleBuzz(int durationMs) {
  digitalWrite(MOTOR_PIN, HIGH);
  delay(durationMs);
  digitalWrite(MOTOR_PIN, LOW);
}

// Two quick beats close together, like a heartbeat
void heartbeat() {
  for (int i = 0; i < 3; i++) {
    singleBuzz(80);
    delay(60);
    singleBuzz(80);
    delay(400);
  }
}

// PWM ramp: gradually increase then decrease motor intensity
// Note: BC547 + small ERM motor will produce noticeable intensity
// variation. The IRF520N would do this less effectively at 3.3V.
void rampUpDown() {
  // Ramp up
  for (int duty = 0; duty <= 255; duty += 5) {
    analogWrite(MOTOR_PIN, duty);
    delay(15);
  }
  // Hold at full for a moment
  analogWrite(MOTOR_PIN, 255);
  delay(300);
  // Ramp down
  for (int duty = 255; duty >= 0; duty -= 5) {
    analogWrite(MOTOR_PIN, duty);
    delay(15);
  }
  analogWrite(MOTOR_PIN, 0);
}
