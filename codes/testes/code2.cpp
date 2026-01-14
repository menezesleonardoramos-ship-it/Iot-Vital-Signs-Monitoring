#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "MAX30105.h"
#include "heartRate.h"
#include <WiFi.h>
#include <Firebase_ESP_Client.h>
#include <time.h>

// ================= OLED =================
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// ================= MAX30102 =================
MAX30105 particleSensor;
const byte RATE_SIZE = 4;
byte rates[RATE_SIZE];
byte rateSpot = 0;
long lastBeat = 0;
float beatsPerMinute;
int beatAvg;
int spo2;

// ================= MAX30205 =================
#define MAX30205_ADDRESS 0x48
float temperatura;

// ================= WIFI & FIREBASE =================
#define WIFI_SSID "Emerson"
#define WIFI_PASSWORD "guebas123"
#define DATABASE_URL "projeto-integradora-ii-default-rtdb.firebaseio.com"
#define DATABASE_SECRET "uC0SyV22Gi3CEPqkslCwpJtPB1Kr2EAiH9JmrMVD"

FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;
bool timeSynced = false;
unsigned long lastFirebaseSend = 0;
const unsigned long FIREBASE_INTERVAL = 10000; // 10 segundos

// ================= PROTÓTIPOS OTIMIZADOS =================
void initOLED();
void initMAX30102();
float readMAX30205();
void initWiFi();
void initFirebase();
void sendToFirebase();
void updateDisplay();

void setup() {
  Serial.begin(115200);
  Wire.begin();
  
  initOLED();
  initMAX30102();
  initWiFi();
  initFirebase();
  
  // Configura NTP (não-bloqueante)
  configTime(-3 * 3600, 0, "pool.ntp.org", "time.nist.gov");
}

void loop() {
  // Leitura contínua do sensor MAX30102
  long irValue = particleSensor.getIR();
  
  if (irValue > 50000) {
    if (checkForBeat(irValue) == true) {
      long delta = millis() - lastBeat;
      lastBeat = millis();
      beatsPerMinute = 60 / (delta / 1000.0);
      
      if (beatsPerMinute < 255 && beatsPerMinute > 20) {
        rates[rateSpot++] = (byte)beatsPerMinute;
        rateSpot %= RATE_SIZE;
        
        // Calcula média
        beatAvg = 0;
        for (byte x = 0; x < RATE_SIZE; x++)
          beatAvg += rates[x];
        beatAvg /= RATE_SIZE;
      }
    }
    
    // Leitura de SpO2 (simplificada)
    spo2 = 95 + random(-2, 3); // Placeholder - implemente algoritmo real se necessário
  }
  
  // Leitura de temperatura a cada 2 segundos
  static unsigned long lastTempRead = 0;
  if (millis() - lastTempRead > 2000) {
    temperatura = readMAX30205();
    lastTempRead = millis();
  }
  
  // Atualiza display constantemente
  updateDisplay();
  
  // Envia para Firebase a cada intervalo
  if (millis() - lastFirebaseSend > FIREBASE_INTERVAL) {
    sendToFirebase();
    lastFirebaseSend = millis();
  }
  
  delay(10); // Delay mínimo para estabilidade
}

// ================= FUNÇÕES INICIALIZAÇÃO =================
void initOLED() {
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("OLED falhou"));
    for(;;);
  }
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0,0);
  display.println("Sinais Vitais");
  display.display();
  delay(1000);
}

void initMAX30102() {
  if (!particleSensor.begin(Wire, I2C_SPEED_FAST)) {
    Serial.println("MAX30102 falhou");
    while (1);
  }
  particleSensor.setup();
  particleSensor.setPulseAmplitudeRed(0x0A);
  particleSensor.setPulseAmplitudeGreen(0);
}

float readMAX30205() {
  Wire.beginTransmission(MAX30205_ADDRESS);
  Wire.write(0x00); // Registro de temperatura
  Wire.endTransmission(false);
  Wire.requestFrom(MAX30205_ADDRESS, 2, true);
  
  byte msb = Wire.read();
  byte lsb = Wire.read();
  
  int16_t temp = (msb << 8) | lsb;
  return temp * 0.00390625; // Conversão para Celsius
}

void initWiFi() {
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Conectando WiFi");
  
  int timeout = 0;
  while (WiFi.status() != WL_CONNECTED && timeout < 20) {
    delay(500);
    Serial.print(".");
    timeout++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi OK!");
  } else {
    Serial.println("\nWiFi falhou!");
  }
}

void initFirebase() {
  config.database_url = DATABASE_URL;
  config.signer.tokens.legacy_token = DATABASE_SECRET;
  
  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);
  
  if (Firebase.ready()) {
    Serial.println("Firebase OK!");
  }
}

// ================= FUNÇÕES PRINCIPAIS =================
void updateDisplay() {
  display.clearDisplay();
  display.setCursor(0,0);
  
  display.print("Temp: ");
  display.print(temperatura, 1);
  display.println(" C");
  
  display.print("BPM: ");
  if (beatAvg > 0) {
    display.println(beatAvg);
  } else {
    display.println("--");
  }
  
  display.print("SpO2: ");
  display.print(spo2);
  display.println(" %");
  
  display.print("IR: ");
  display.println(particleSensor.getIR());
  
  display.display();
}

void sendToFirebase() {
  if (!Firebase.ready() || WiFi.status() != WL_CONNECTED) {
    return;
  }
  
  // Obtém timestamp simplificado
  time_t now = time(nullptr);
  if (now < 24*3600) return; // Tempo não sincronizado
  
  char timestamp[20];
  strftime(timestamp, 20, "%Y%m%d_%H%M%S", localtime(&now));
  
  // Envia dados atuais
  Firebase.RTDB.setFloat(&fbdo, "/atuais/temp", temperatura);
  Firebase.RTDB.setInt(&fbdo, "/atuais/bpm", beatAvg);
  Firebase.RTDB.setInt(&fbdo, "/atuais/spo2", spo2);
  
  // Envia histórico (apenas timestamp + dados)
  String path = "/historico/" + String(timestamp);
  FirebaseJson json;
  json.set("temp", temperatura);
  json.set("bpm", beatAvg);
  json.set("spo2", spo2);
  
  Firebase.RTDB.setJSON(&fbdo, path.c_str(), &json);
  
  Serial.println("Dados enviados!");
}