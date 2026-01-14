#include <Wire.h>
#include "MAX30105.h"
#include "spo2_algorithm.h"
#include "heartRate.h"

MAX30105 particleSensor;

uint32_t irBuffer[100];
uint32_t redBuffer[100];

int32_t bufferLength;
int32_t spo2;
int8_t validSPO2;
int32_t heartRate;
int8_t validHeartRate;

bool initialized = false;

void setup()
{
  Wire.begin(4, 15);
  Wire.setClock(400000);
  Serial.begin(115200);
  Serial.println("Inicializando sensor...");

  if (!particleSensor.begin(Wire, I2C_SPEED_FAST))
  {
    Serial.println("Sensor MAX30105 nao encontrado!");
    while (1);
  }

  byte ledBrightness = 50;
  byte sampleAverage = 1;
  byte ledMode = 2;
  byte sampleRate = 100;
  int pulseWidth = 69;
  int adcRange = 4096;
  
  particleSensor.setup(ledBrightness, sampleAverage, ledMode, sampleRate, pulseWidth, adcRange);
  Serial.println("Sensor inicializado com sucesso!");

  // Coleta 100 amostras iniciais apenas uma vez
  bufferLength = 100;
  for (byte i = 0; i < bufferLength; i++)
  {
    while (!particleSensor.available())
      particleSensor.check();
  
    redBuffer[i] = particleSensor.getIR();
    irBuffer[i] = particleSensor.getRed();
    particleSensor.nextSample();
  }
  
  // Calcula batimento e oxigenação inicial
  maxim_heart_rate_and_oxygen_saturation(irBuffer, bufferLength, redBuffer, &spo2, &validSPO2, &heartRate, &validHeartRate);
  
  initialized = true;
}

void loop()
{
  if (!initialized) return;
  
  // Atualiza buffer deslocando os valores
  for (byte i = 25; i < 100; i++)
  {
    redBuffer[i - 25] = redBuffer[i];
    irBuffer[i - 25] = irBuffer[i];
  }

  // Coleta 25 novas amostras
  for (byte i = 75; i < 100; i++)
  {
    while (!particleSensor.available())
      particleSensor.check();
  
    redBuffer[i] = particleSensor.getRed();
    irBuffer[i] = particleSensor.getIR();
    particleSensor.nextSample();
  }

  // Recalcula valores
  maxim_heart_rate_and_oxygen_saturation(irBuffer, bufferLength, redBuffer, &spo2, &validSPO2, &heartRate, &validHeartRate);
  
  // Exibe resultados no console
  if (validHeartRate == 1 && heartRate > 0) {
    Serial.print("BPM: ");
    Serial.print(heartRate);
  } else {
    Serial.print("BPM: --");
  }
  
  if (validSPO2 == 1 && spo2 > 0 && spo2 <= 100) {
    Serial.print("\tSPO2: ");
    Serial.print(spo2);
    Serial.println("%");
  } else {
    Serial.println("\tSPO2: --%");
  }
  
  delay(1000); // Aguarda 1 segundo entre as leituras
}