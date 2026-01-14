#include <Wire.h>

void setup() {
  Serial.begin(115200);
  Wire.begin(21, 22); // SDA=21, SCL=22
  delay(100);
  Serial.println("I2C Scanner");
  for (uint8_t addr = 1; addr < 127; addr++) {
    Wire.beginTransmission(addr);
    uint8_t err = Wire.endTransmission();
    if (err == 0) {
      Serial.print("Device found at 0x");
      Serial.println(addr, HEX);
    }
  }
}

void loop(){ delay(10000); }
