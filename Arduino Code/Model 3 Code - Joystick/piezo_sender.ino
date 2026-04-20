/* =============================================================
   PIEZO SENDER BOARD — Adafruit Feather nRF52840 Express
   -------------------------------------------------------------
   • Role: BLE PERIPHERAL
   • Advertises as "PiezoSender" with BLEUart service UUID
   • Collects a window of samples, sends only the peak
   • Dead zone suppresses sends when signal is stable / at rest
   ============================================================= */

#include <bluefruit.h>

/* =======================
   Pin / Settings
   ======================= */
#define PIEZO_PIN           A1
#define LED_PIN             LED_RED

// Sampling
#define SAMPLE_INTERVAL_MS  5      // how often to read the ADC
#define WINDOW_SIZE         10     // samples per window (10 * 5ms = 50ms window)

// Filtering
#define DEAD_ZONE           8      // don't send if change < this from last sent value
#define NOISE_FLOOR         6      // treat anything at or below this as zero / idle

/* =======================
   BLE Peripheral UART Service
   ======================= */
BLEDis  bledis;
BLEUart bleuart;

/* =======================
   State
   ======================= */
bool     bleConnected  = false;
uint32_t lastSample    = 0;
int      windowPeak    = 0;
int      sampleCount   = 0;
int      lastSentValue = -1;

/* =======================
   BLE Peripheral Callbacks
   ======================= */
void connect_callback(uint16_t conn_handle) {
  (void) conn_handle;
  bleConnected   = true;
  lastSentValue  = -1;
  windowPeak     = 0;
  sampleCount    = 0;
  Serial.println("[BLE] Central connected — sending piezo data!");
}

void disconnect_callback(uint16_t conn_handle, uint8_t reason) {
  (void) conn_handle;
  bleConnected = false;
  Serial.print("[BLE] Disconnected. Reason: 0x");
  Serial.println(reason, HEX);
}

/* =======================
   Start Advertising
   ======================= */
void startAdv() {
  Bluefruit.Advertising.addFlags(BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE);
  Bluefruit.Advertising.addTxPower();
  Bluefruit.Advertising.addService(bleuart);
  Bluefruit.ScanResponse.addName();
  Bluefruit.Advertising.restartOnDisconnect(true);
  Bluefruit.Advertising.setInterval(32, 244);
  Bluefruit.Advertising.setFastTimeout(30);
  Bluefruit.Advertising.start(0);
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
  Serial.println(" Piezo Sender — BLE Peripheral");
  Serial.println(" Advertising as 'PiezoSender'...");
  Serial.println("============================================");

  analogReadResolution(10);
  pinMode(PIEZO_PIN, INPUT);

  // Peripheral = 1, Central = 0
  Bluefruit.begin(1, 0);
  Bluefruit.setTxPower(4);
  Bluefruit.setName("PiezoSender");

  Bluefruit.Periph.setConnectCallback(connect_callback);
  Bluefruit.Periph.setDisconnectCallback(disconnect_callback);

  bledis.setManufacturer("Adafruit Industries");
  bledis.setModel("Feather nRF52840");
  bledis.begin();

  bleuart.begin();
  startAdv();

  Serial.println("[BLE] Advertising — waiting for central to connect...");
}

/* =======================
   Loop
   ======================= */
void loop() {
  uint32_t now = millis();

  if (now - lastSample >= SAMPLE_INTERVAL_MS) {
    lastSample = now;

    int raw = analogRead(PIEZO_PIN);
    if (raw <= NOISE_FLOOR) raw = 0;

    if (raw > windowPeak) windowPeak = raw;
    sampleCount++;

    if (sampleCount >= WINDOW_SIZE) {
      int toSend = windowPeak;

      Serial.print("[WINDOW PEAK] "); Serial.println(toSend);

      if (bleConnected) {
        bool isResting   = (toSend <= NOISE_FLOOR);
        bool wasResting  = (lastSentValue <= NOISE_FLOOR);
        bool changeLarge = (abs(toSend - lastSentValue) >= DEAD_ZONE);

        if (lastSentValue < 0 || changeLarge || (isResting != wasResting)) {
          char buffer[12];
          snprintf(buffer, sizeof(buffer), "%d\n", toSend);
          bleuart.write(buffer, strlen(buffer));
          lastSentValue = toSend;
          Serial.print("[SENT] "); Serial.print(buffer);
        } else {
          Serial.println("[SKIP] Dead zone — no significant change");
        }
      }

      windowPeak  = 0;
      sampleCount = 0;
    }
  }

  delay(1);
}
