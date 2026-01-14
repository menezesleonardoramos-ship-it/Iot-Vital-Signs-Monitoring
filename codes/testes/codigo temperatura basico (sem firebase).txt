#include <Wire.h>

#define SDA_PIN 21
#define SCL_PIN 22
#define ADDR 0x48

void setup() {
  Serial.begin(115200);
  Wire.begin(SDA_PIN, SCL_PIN);
  delay(50);
  Serial.println("Leitura direta MAX30205 -> conversao em °C");
}

float readMAX30205() {
  Wire.beginTransmission(ADDR);
  Wire.write(0x00);
  if (Wire.endTransmission(false) != 0) return NAN; //

  Wire.requestFrom(ADDR, (uint8_t)2);
  if (Wire.available() < 2) return NAN;

  uint8_t msb = Wire.read();
  uint8_t lsb = Wire.read();
  uint16_t raw = ((uint16_t)msb << 8) | lsb;

  int16_t signedRaw = (raw & 0x8000) ? (int16_t)(raw - 65536) : (int16_t)raw;


  float temp = ((float)signedRaw) / 256.0;

  return temp;
}

void loop() {
  float t = readMAX30205();
  if (!isnan(t)) {
    Serial.print("Temperatura: ");
    Serial.print(t, 6); 
    Serial.println(" °C");
  } else {
    Serial.println("Erro de I2C / leitura incompleta");
  }
  delay(1000);
}