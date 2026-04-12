/* =============================================================
   SERVO RECEIVER BOARD — Adafruit Feather nRF52840 Express
   + Adafruit PWM Servo FeatherWing
   -------------------------------------------------------------
   • Role: BLE PERIPHERAL only (Central role to be added later)
   • Advertises as "ServoFeather" with BLEUart service UUID
   • Receives raw piezo ADC values from PiezoSender board
   • Maps piezoVal → servo pulse width on channel 7
     (threshold + power-curve for reduced sensitivity)
   • BLEUart RX is callback-driven (not polled in loop)
   • Serial Monitor: '?' = status, 'r' = reset servo
   • BLE UART commands: '?' = status, 'U' = up, 'D' = down,
                        'C' = centre/reset, '1' = reset + status
   ============================================================= */

#include <Wire.h>
#include <Adafruit_PWMServoDriver.h>
#include <bluefruit.h>

/* =======================
   PCA9685 Servo Driver
   ======================= */
Adafruit_PWMServoDriver pwm = Adafruit_PWMServoDriver();

/* =======================
   Servo Configuration
   ======================= */
#define SERVO_FREQ   50
#define USMIN        1000
#define USMAX        2000
#define SERVO_STEP   100

/* =======================
   Sensitivity Tuning
   -----------------------
   PIEZO_THRESHOLD : readings at or below this are ignored (filters
                     noise and light vibration). Range 0–1023.
   CURVE_EXPONENT  : power applied to the normalised piezo value
                     before mapping to servo position.
                     1.0 = linear, 2.0 = quadratic (default),
                     3.0+ = requires a harder press for same movement.
   ======================= */
#define PIEZO_THRESHOLD  10    // filter only true zero readings
#define PIEZO_MAX        60
#define CURVE_EXPONENT   2.0f

uint8_t servoNum  = 7;
int     currentUs = USMAX;

/* =======================
   Pin / LED
   ======================= */
#define LED_PIN  LED_RED   // LOW = ON on nRF52840

/* =======================
   BLE Services
   -----------------------
   Peripheral = 1, Central = 0  (Central role added later → change to (1,1))
   ======================= */
BLEDis  bledis;
BLEUart bleuart;

/* =======================
   State Tracking
   ======================= */
bool     bleConnected  = false;
uint32_t lastConnTime  = 0;
uint32_t lastPiezoTime = 0;
int      lastPiezoVal  = -1;

/* =======================
   Forward declarations
   ======================= */
void printStatus();

/* =======================
   Servo Helpers
   ======================= */
void moveServo(int newUs) {
  currentUs = constrain(newUs, USMIN, USMAX);
  pwm.writeMicroseconds(servoNum, currentUs);
  Serial.print("[SERVO] -> ");
  Serial.println(currentUs);
}

void resetServo() {
  moveServo(USMAX);
  Serial.println("[SERVO] Reset to USMAX");
}

/* =======================
   Status — Serial + BLE
   ======================= */
void printStatus() {
  String s = "";
  s += "=== ServoFeather Status ===\n";
  s += "BLE connected  : ";
  s += bleConnected ? "YES" : "NO";
  s += "\n";
  if (bleConnected) {
    s += "Connected for  : ";
    s += String((millis() - lastConnTime) / 1000);
    s += "s\n";
  }
  s += "Servo pos (us) : ";
  s += String(currentUs);
  s += "\n";
  s += "Last piezo val : ";
  if (lastPiezoVal < 0) {
    s += "none yet\n";
  } else {
    s += String(lastPiezoVal);
    s += " (";
    s += String((millis() - lastPiezoTime) / 1000);
    s += "s ago)\n";
  }
  s += "===========================";
  Serial.println(s);
  if (bleConnected) {
    bleuart.print(s);
    bleuart.print("\n");
  }
}

/* =======================
   Process a single command character
   (shared by BLE and Serial paths)
   ======================= */
void handleCommand(char cmd) {
  switch (cmd) {
    case 'U': moveServo(currentUs + SERVO_STEP); break;
    case 'D': moveServo(currentUs - SERVO_STEP); break;
    case 'C': resetServo();                       break;
    case '1': resetServo(); printStatus();         break;
    case '?': printStatus();                       break;
    case '\n': case '\r':                          break;
    default:
      Serial.print("[CMD] Unknown: '");
      Serial.print(cmd);
      Serial.println("'");
      if (bleConnected) bleuart.print("Unknown cmd. Send '?' for help.\n");
      break;
  }
}

/* =======================
   BLE UART RX Callback
   -----------------------
   Called automatically by the Bluefruit library whenever data
   arrives — no polling needed in loop().
   ======================= */
void bleuart_rx_callback(uint16_t conn_handle) {
  (void) conn_handle;

  // Peek at the first byte to decide numeric vs command
  if (!bleuart.available()) return;
  char c = bleuart.peek();

  if (isdigit(c)) {
    // Numeric string from PiezoSender: read until newline
    String incoming = bleuart.readStringUntil('\n');
    int piezoVal    = incoming.toInt();

    lastPiezoVal  = piezoVal;
    lastPiezoTime = millis();

    Serial.print("[PIEZO] Received: ");
    Serial.println(piezoVal);

    // Option 1 — threshold: ignore taps at or below PIEZO_THRESHOLD
    if (piezoVal <= PIEZO_THRESHOLD) {
      Serial.println("[PIEZO] Below threshold — ignored");
      return;
    }

    float norm   = (float)(piezoVal - PIEZO_THRESHOLD) / (PIEZO_MAX - PIEZO_THRESHOLD);
    norm         = constrain(norm, 0.0f, 1.0f);
    norm         = pow(norm, CURVE_EXPONENT);
    int targetUs = USMAX - (int)(norm * (USMAX - USMIN));
    moveServo(targetUs);
  } else {
    // Single-character command (e.g. from Bluefruit app)
    char cmd = (char) bleuart.read();
    Serial.print("[CMD] BLE: '");
    Serial.print(cmd);
    Serial.println("'");
    handleCommand(cmd);
  }
}

/* =======================
   BLE Peripheral Callbacks
   ======================= */
void connect_callback(uint16_t conn_handle) {
  (void) conn_handle;
  bleConnected = true;
  lastConnTime = millis();
  Serial.println("[BLE] Peripheral connected");
  bleuart.print("ServoFeather connected! Send '?' for status.\n");
}

void disconnect_callback(uint16_t conn_handle, uint8_t reason) {
  (void) conn_handle;
  bleConnected = false;
  Serial.print("[BLE] Peripheral disconnected. Reason: 0x");
  Serial.println(reason, HEX);
  // Advertising.restartOnDisconnect(true) below handles auto-restart
}

/* =======================
   Handle Serial Input
   ======================= */
void handleSerialInput() {
  if (!Serial.available()) return;
  char cmd = Serial.read();
  switch (cmd) {
    case '?':           printStatus(); break;
    case 'r': case 'R': resetServo();  break;
    case '\n': case '\r':              break;
    default:
      Serial.println("Serial cmds: '?' = status, 'r' = reset servo");
      break;
  }
}

/* =======================
   Start Advertising Helper
   (mirrors Dual_Example.ino startAdv())
   ======================= */
void startAdv() {
  Bluefruit.Advertising.addFlags(BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE);
  Bluefruit.Advertising.addTxPower();
  Bluefruit.Advertising.addService(bleuart);   // include BLEUart UUID in primary packet
  Bluefruit.ScanResponse.addName();            // name in scan response packet

  Bluefruit.Advertising.restartOnDisconnect(true);
  Bluefruit.Advertising.setInterval(32, 244);  // fast=20 ms, slow=152.5 ms (0.625 ms units)
  Bluefruit.Advertising.setFastTimeout(30);    // 30 s in fast mode
  Bluefruit.Advertising.start(0);             // advertise forever
}

/* =======================
   Setup
   ======================= */
void setup() {
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  Serial.begin(115200);
  delay(1500);
  Serial.println("============================================");
  Serial.println(" Servo BLE PWM Board Starting...");
  Serial.println(" Serial cmds: '?' = status, 'r' = reset servo");
  Serial.println("============================================");

  /* ---- PWM / Servo init ---- */
  pwm.begin();
  pwm.setOscillatorFrequency(27000000);
  pwm.setPWMFreq(SERVO_FREQ);
  delay(10);
  resetServo();

  /* ---- BLE init ----
   * Peripheral = 1, Central = 0
   * When Central role is added later, change to Bluefruit.begin(1, 1)
   * and add the Central scanning block from Dual_Example.ino.
   */
  Bluefruit.begin(1, 0);
  Bluefruit.setTxPower(4);
  Bluefruit.setName("ServoFeather");

  /* ---- Peripheral callbacks ---- */
  Bluefruit.Periph.setConnectCallback(connect_callback);
  Bluefruit.Periph.setDisconnectCallback(disconnect_callback);

  /* ---- Device Information Service ---- */
  bledis.setManufacturer("Adafruit Industries");
  bledis.setModel("Feather nRF52840");
  bledis.begin();

  /* ---- BLE UART Service ----
   * setRxCallback() replaces the polling approach; the library calls
   * bleuart_rx_callback() automatically when data arrives.
   */
  bleuart.begin();
  bleuart.setRxCallback(bleuart_rx_callback);

  /* ---- Start advertising ---- */
  startAdv();

  Serial.println("[BLE] Advertising as 'ServoFeather'");
  Serial.println("Ready.");
}

/* =======================
   Loop
   -----------------------
   All BLE work is done in callbacks. Loop only handles Serial input.
   ======================= */
void loop() {
  handleSerialInput();
  delay(5);
}
