#include <Wire.h>
#include <U8g2lib.h>
#include "MAX30105.h"
#include <WiFi.h>
#include <Firebase_ESP_Client.h>
#include <time.h>

/* ================= OLED ================= */
U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);

/* ================= MAX30102 ================= */
MAX30105 particleSensor;

/* ================= BPM ================= */
const byte RATE_SIZE = 6;
byte rates[RATE_SIZE];
byte rateIndex = 0;
int bpmAvg = 0;

float irFiltered = 0;
long irAvg = 0;
long irPrev = 0;
bool rising = false;
unsigned long lastBeatTime = 0;

#define IR_ALPHA 0.1

/* ================= SpO2 ================= */
long redAvg = 0;
long irAvgSpO2 = 0;
int spo2 = 0;

/* ================= MAX30205 ================= */
#define MAX30205_ADDRESS 0x48
float temperatura = 36.5;
unsigned long lastTempRead = 0;

/* ================= WIFI & FIREBASE ================= */
#define WIFI_SSID "PET Convidados"
#define WIFI_PASSWORD "petagregado"
#define DATABASE_URL "projeto-integradora-ii-default-rtdb.firebaseio.com"
#define DATABASE_SECRET "uC0SyV22Gi3CEPqkslCwpJtPB1Kr2EAiH9JmrMVD"

FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

unsigned long lastFirebaseSend = 0;
const unsigned long FIREBASE_INTERVAL = 10000;

/* ============================================================= */

void setup() {
  Serial.begin(115200);
  Wire.begin(21, 22);
  Wire.setClock(100000);

  u8g2.begin();
  u8g2.setFont(u8g2_font_6x10_tr);

  if (!particleSensor.begin(Wire, I2C_SPEED_FAST)) {
    displayMessage("ERRO", "Sensor nao", "encontrado");
    while (1);
  }

  particleSensor.setup(0x1F, 4, 2, 200, 215, 2048);
  particleSensor.setPulseAmplitudeIR(0x0F);
  particleSensor.setPulseAmplitudeRed(0x0F);

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  config.database_url = DATABASE_URL;
  config.signer.tokens.legacy_token = DATABASE_SECRET;
  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);

  configTime(-3 * 3600, 0, "pool.ntp.org");

  displayMessage("Aguardando", "coloque o", "dedo");
  delay(2000);
}

/* ============================================================= */

void loop() {
  unsigned long now = millis();

  long irValue = particleSensor.getIR();
  long redValue = particleSensor.getRed();

  /* ============ FILTRO IR ============ */
  irFiltered = irFiltered * (1.0 - IR_ALPHA) + irValue * IR_ALPHA;

  /* ============ BPM SEM PICOS FALSOS ============ */
  irAvg = irAvg * 0.98 + irFiltered * 0.02;
  long signal = irFiltered - irAvg;
  long dynamicThreshold = max(1200L, irAvg * 0.015);

  if (signal > dynamicThreshold && irPrev < signal && !rising) {
    rising = true;
  }

  if (rising && signal < irPrev) {
    rising = false;

    if (lastBeatTime > 0) {
      unsigned long delta = now - lastBeatTime;

      if (delta > 300 && delta < 1500) { // 40–200 BPM
        float bpm = 60000.0 / delta;

        rates[rateIndex++] = (byte)bpm;
        rateIndex %= RATE_SIZE;

        bpmAvg = 0;
        for (byte i = 0; i < RATE_SIZE; i++) bpmAvg += rates[i];
        bpmAvg /= RATE_SIZE;

        Serial.print("BPM: ");
        Serial.println(bpmAvg);
      }
    }
    lastBeatTime = now;
  }
  irPrev = signal;

  /* ============ SpO2 ESTÁVEL ============ */
  irAvgSpO2 = irAvgSpO2 * 0.95 + irValue * 0.05;
  redAvg    = redAvg    * 0.95 + redValue * 0.05;

  long irAC  = irValue  - irAvgSpO2;
  long redAC = redValue - redAvg;

  if (irAC > 700 && redAC > 700 && irAvgSpO2 > 20000) {
    float ratio = (float(redAC) / redAvg) / (float(irAC) / irAvgSpO2);
    spo2 = constrain(110 - 25 * ratio, 85, 100);
  }

  /* ============ TEMPERATURA ============ */
  if (now - lastTempRead > 1000) {
    float t = readMAX30205();
    if (!isnan(t)) temperatura = t;
    lastTempRead = now;
  }

  updateDisplay(irValue);

  /* ============ FIREBASE ============ */
  if (now - lastFirebaseSend > FIREBASE_INTERVAL) {
    sendToFirebase();
    lastFirebaseSend = now;
  }
}

/* ============================================================= */

float readMAX30205() {
  const int samples = 10;
  float soma = 0;
  int valid = 0;
  static float tempFiltrada = 36.5;

  for (int i = 0; i < samples; i++) {
    Wire.beginTransmission(MAX30205_ADDRESS);
    Wire.write(0x00);
    if (Wire.endTransmission(false) != 0) continue;
    if (Wire.requestFrom(MAX30205_ADDRESS, 2) != 2) continue;

    int16_t raw = (Wire.read() << 8) | Wire.read();
    float temp = raw * 0.00390625;

    if (temp >= 32.0 && temp <= 42.5) {
      soma += temp;
      valid++;
    }
  }

  if (valid == 0) return NAN;

  float media = soma / valid;
  tempFiltrada = tempFiltrada * 0.85 + media * 0.15;
  return tempFiltrada;
}

/* ============================================================= */

void updateDisplay(long ir) {
  u8g2.clearBuffer();

  u8g2.setCursor(0, 12);
  u8g2.print("Temp: ");
  u8g2.print(temperatura, 1);
  u8g2.print(" C");

  u8g2.setCursor(0, 24);
  u8g2.print("BPM: ");
  bpmAvg > 0 ? u8g2.print(bpmAvg) : u8g2.print("--");

  u8g2.setCursor(0, 36);
  u8g2.print("SpO2: ");
  spo2 > 0 ? u8g2.print(spo2) : u8g2.print("--");
  u8g2.print(" %");

  u8g2.setCursor(0, 48);
  u8g2.print("IR: ");
  u8g2.print(ir);

  u8g2.sendBuffer();
}

/* ============================================================= */

void sendToFirebase() {
  if (!Firebase.ready()) return;

  Firebase.RTDB.setFloat(&fbdo, "/atuais/temp", temperatura);
  Firebase.RTDB.setInt(&fbdo, "/atuais/bpm", bpmAvg);
  Firebase.RTDB.setInt(&fbdo, "/atuais/spo2", spo2);

  Serial.println("Firebase atualizado");
}

void displayMessage(const char* l1, const char* l2, const char* l3) {
  u8g2.clearBuffer();
  u8g2.drawStr(10, 20, l1);
  u8g2.drawStr(10, 35, l2);
  u8g2.drawStr(10, 50, l3);
  u8g2.sendBuffer();
}
