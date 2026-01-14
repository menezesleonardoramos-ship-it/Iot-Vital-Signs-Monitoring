#include <Wire.h>
#include <U8g2lib.h>
#include "MAX30105.h"

// OLED
U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);

// Sensor (MAX30102 usa a mesma lib)
MAX30105 particleSensor;

// ================== PARÂMETROS DO ALGORITMO ==================
#define IR_THRESHOLD     800     // sensibilidade do pico
#define MIN_BEAT_INTERVAL 300     // ms (200 BPM máx)
#define MAX_BEAT_INTERVAL 2000    // ms (30 BPM mín)

// ================== VARIÁVEIS ==================
long irValue;
long irFiltered = 0;
long irPrev = 0;

unsigned long lastBeatTime = 0;
float bpm = 0;
int bpmAvg = 0;

const byte RATE_SIZE = 6;
byte rates[RATE_SIZE];
byte rateIndex = 0;

// =============================================================

void setup() {
  Serial.begin(115200);

  Wire.begin(21, 22);

  u8g2.begin();
  u8g2.setFont(u8g2_font_6x10_tr);

  delay(1000);

  if (!particleSensor.begin(Wire, I2C_SPEED_FAST)) {
    displayMessage("ERRO", "Sensor nao", "encontrado");
    while (1);
  }

  // CONFIGURAÇÃO IDEAL PARA MAX30102
  particleSensor.setup(
    0x1F,   // brilho do LED
    4,      // média de amostras
    2,      // RED + IR
    200,    // sample rate
    215,    // pulse width
    2048    // ADC range
  );

  particleSensor.setPulseAmplitudeIR(0x0F);
  particleSensor.setPulseAmplitudeRed(0x00);

  displayMessage("Aguardando", "coloque o", "dedo");
  delay(2000);
}

// =============================================================

void loop() {

  long irValue = particleSensor.getIR();
  unsigned long now = millis();

  static long irPrev = 0;
  static long irAvg = 0;
  static bool rising = false;
  static unsigned long lastBeatTime = 0;

  // Média móvel simples (baseline)
  irAvg = irAvg * 0.95 + irValue * 0.05;

  long signal = irValue - irAvg;

  // Detecção de pico (máximo local)
  if (signal > 1500 && irPrev < signal && !rising) {
    rising = true;
  }

  if (rising && signal < irPrev) {
    // Pico detectado
    rising = false;

    if (lastBeatTime > 0) {
      unsigned long delta = now - lastBeatTime;
      float bpm = 60000.0 / delta;

      if (bpm > 40 && bpm < 180) {
        rates[rateIndex++] = (byte)bpm;
        rateIndex %= RATE_SIZE;

        bpmAvg = 0;
        for (byte i = 0; i < RATE_SIZE; i++)
          bpmAvg += rates[i];
        bpmAvg /= RATE_SIZE;

        Serial.print("BATIMENTO | BPM: ");
        Serial.println(bpmAvg);
      }
    }

    lastBeatTime = now;
  }

  irPrev = signal;

  // Display
  bool hasFinger = irValue > 70000;
  updateDisplay(irValue, hasFinger);

  delay(10);
}


// =============================================================

void updateDisplay(long ir, bool hasFinger) {
  u8g2.clearBuffer();

  u8g2.setFont(u8g2_font_7x14B_tr);
  u8g2.drawStr(10, 15, "BPM Monitor");

  u8g2.setFont(u8g2_font_5x8_tr);
  char irStr[20];
  sprintf(irStr, "IR: %ld", ir);
  u8g2.drawStr(10, 30, irStr);

  u8g2.setFont(u8g2_font_6x10_tr);
  if (hasFinger)
    u8g2.drawStr(10, 45, "Dedo: Detectado");
  else
    u8g2.drawStr(10, 45, "Dedo: Ausente");

  u8g2.setFont(u8g2_font_fub20_tn);
  if (hasFinger && bpmAvg > 0) {
    char bpmStr[10];
    sprintf(bpmStr, "%d", bpmAvg);
    u8g2.drawStr(40, 65, bpmStr);
  } else {
    u8g2.drawStr(40, 65, "--");
  }

  u8g2.sendBuffer();
}

// =============================================================

void displayMessage(const char* l1, const char* l2, const char* l3) {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tr);
  u8g2.drawStr(10, 20, l1);
  u8g2.drawStr(10, 35, l2);
  u8g2.drawStr(10, 50, l3);
  u8g2.sendBuffer();
}
