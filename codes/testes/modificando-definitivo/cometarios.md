
# Explicação código 
---

## SEÇÃO DE INCLUSÃO DE BIBLIOTECAS

```cpp
#include <Wire.h>           // Biblioteca para comunicação I2C
#include <U8g2lib.h>        // Biblioteca para controle do display OLED
#include "MAX30105.h"       // Biblioteca para sensor de batimento cardíaco e SpO2
#include <WiFi.h>           // Biblioteca WiFi para ESP32
#include <Firebase_ESP_Client.h>  // Biblioteca para integração com Firebase
#include <time.h>           // Biblioteca para manipulação de tempo
```
---

## INICIALIZAÇÃO DE DISPLAY E SENSORES

```cpp
// Display OLED - Objeto para controlar display SSD1306 128x64 via I2C
// U8G2_R0: Rotação normal (0 graus)
// U8X8_PIN_NONE: Sem pino de reset dedicado
U8G2_SSD1306_128X64_NONAME_F_HW_I2C oled(U8G2_R0, U8X8_PIN_NONE);

// Sensor MAX30102 - Para medição de BPM e SpO2
MAX30105 particleSensor;
```



---

## VARIÁVEIS PARA CÁLCULO DE BPM


```cpp
const byte RATE_SIZE = 6;   // Tamanho do array para média móvel (6 valores)
byte rates[RATE_SIZE];      // Array para armazenar valores de BPM
byte rateIndex = 0;         // Índice atual no array
int bpmAvg = 0;             // Média dos valores de BPM

// Variáveis para detecção de batimentos
static long irAvg = 0;      // Média móvel do sinal IR
static long irPrev = 0;     // Valor anterior do sinal IR
static bool rising = false; // Flag indicando se sinal está subindo
static unsigned long lastBeatTime = 0; // Tempo do último batimento detectado
```



### Explicação

---

# VARIÁVEIS PARA CÁLCULO DE SpO2

**Instanciação**

```cpp
// SpO2 
#define SPO2_SIZE 3         // Tamanho do buffer para média móvel do SpO2
int spo2Buffer[SPO2_SIZE];  // Buffer para valores de SpO2
byte spo2Index = 0;         // Índice atual no buffer
int spo2 = 0;               // Valor médio de SpO2
unsigned long lastSpO2Calc = 0; // Último tempo de cálculo do SpO2
```

### Explicação

---

# SENSOR DE TEMPERATURA MAX30205 

```cpp
#define MAX30205_ADDRESS 0x48  // Endereço I2C do sensor de temperatura
float temperatura = 36.5;     // Variável para armazenar temperatura (valor inicial)


```
## CONFIGURAÇÃO WIFI E FIREBASE

```cpp
#define WIFI_SSID "PET Convidados"           // Nome da rede WiFi
#define WIFI_PASSWORD "petagregado"          // Senha da rede WiFi
#define DATABASE_URL "projeto-integradora-ii-default-rtdb.firebaseio.com"  // URL do Firebase
#define DATABASE_SECRET "uC0SyV22Gi3CEPqkslCwpJtPB1Kr2EAiH9JmrMVD"  // Chave secreta do Firebase

FirebaseData fbdo;      // Objeto para transferência de dados com Firebase
FirebaseAuth auth;      // Objeto para autenticação Firebase
FirebaseConfig config;  // Objeto para configuração Firebase

bool firebaseOnline = false;  // Flag indicando status da conexão Firebase
bool sensoresProntos = false; // Flag indicando se sensores estão calibrados
```

### Explicação

---

## CONTROLE DE LOADING E TIMERS

```cpp
// Variáveis para tela de loading inicial
bool telaInicial = true;                 // Flag para tela de loading
unsigned long inicioSistema = 0;         // Marcação de tempo inicial
const unsigned long TEMPO_LOADING = 15000; // Tempo de loading (15 segundos)

// Timers para envio ao Firebase
unsigned long lastFirebaseSend = 0;      // Último envio ao Firebase
const unsigned long FIREBASE_INTERVAL = 5000; // Intervalo entre envios (5 segundos)
```




## FUNÇÃO loadingScreen()

```cpp
void loadingScreen() {
  // Calcula tempo decorrido desde o início
  unsigned long elapsed = millis() - inicioSistema;
  
  // Mapeia tempo para progresso (0-104 pixels)
  int progresso = map(elapsed, 0, TEMPO_LOADING, 0, 104);
  progresso = constrain(progresso, 0, 104); // Limita entre 0 e 104
  
  oled.clearBuffer();  // Limpa buffer do display
  
  // Configura fonte e posições para textos
  oled.setFont(oled_font_6x10_tr);
  
  oled.setCursor(14, 28);
  oled.print("Carregando");
  
  oled.setCursor(14, 40);
  oled.print("Sinais Vitais");
  
  // Desenha barra de progresso
  oled.drawFrame(12, 54, 104, 6);  // Moldura da barra
  oled.drawBox(14, 56, progresso, 2); // Preenchimento da barra
  
  oled.sendBuffer();  // Envia buffer para display
}
```

## FUNÇÃO setup()

```cpp
void setup() {
  Serial.begin(115200);  // Inicia comunicação serial a 115200 baud
  
  // Inicia comunicação I2C nos pinos 21(SDA) e 22(SCL) do ESP32
  Wire.begin(21, 22);
  Wire.setClock(100000);  // Clock I2C a 100kHz
  
  oled.begin();  // Inicializa display OLED
  
  // Configura sensor MAX30102
  particleSensor.begin(Wire, I2C_SPEED_FAST);  // I2C em velocidade rápida
  // Parâmetros: brilho LED, média de amostras, modo LED, taxa, largura pulso, range ADC
  particleSensor.setup(0x1F, 4, 2, 200, 215, 2048);
  particleSensor.setPulseAmplitudeIR(0x0F);  // Amplitude pulso IR
  particleSensor.setPulseAmplitudeRed(0x0F); // Amplitude pulso vermelho
  
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);  // Conecta ao WiFi
  
  // Configura Firebase
  config.database_url = DATABASE_URL;
  config.signer.tokens.legacy_token = DATABASE_SECRET;
  Firebase.begin(&config, &auth);  // Inicia Firebase
  Firebase.reconnectWiFi(true);    // Habilita reconexão automática
  
  configTime(-3 * 3600, 0, "pool.ntp.org");  // Configura fuso horário (UTC-3)
  
  inicioSistema = millis();  // Marca tempo inicial para loading
}
```

## FUNÇÃO loop() - PRINCIPAL

```cpp
void loop() {
  // Se ainda na tela de loading, mostra animação
  if (telaInicial) {
    loadingScreen();
    if (millis() - inicioSistema >= TEMPO_LOADING) {
      telaInicial = false;     // Termina loading
      sensoresProntos = true;  // Sensores prontos
    }
    return;  // Sai da função sem processar sensores
  }
  
  // Lê valores dos sensores
  long irValue = particleSensor.getIR();   // Valor infravermelho
  long redValue = particleSensor.getRed(); // Valor vermelho
  unsigned long now = millis();           // Tempo atual
  
  // --- CÁLCULO DE BPM ---
  irAvg = irAvg * 0.95 + irValue * 0.05;  // Média móvel exponencial (95% antigo + 5% novo)
  long signal = irValue - irAvg;          // Sinal AC (remove componente DC)
  
  // Detecta borda de subida do sinal
  if (signal > 1500 && irPrev < signal && !rising) rising = true;
  
  // Detecta pico (borda de descida)
  if (rising && signal < irPrev) {
    rising = false;  // Reseta flag de subida
    
    if (lastBeatTime > 0) {  // Se não for o primeiro batimento
      float bpm = 60000.0 / (now - lastBeatTime);  // Calcula BPM
      
      if (bpm > 40 && bpm < 180) {  // Filtra valores plausíveis
        rates[rateIndex++] = bpm;    // Armazena no array
        rateIndex %= RATE_SIZE;      // Mantém índice circular
        
        // Calcula média dos últimos 6 valores
        bpmAvg = 0;
        for (byte i = 0; i < RATE_SIZE; i++) bpmAvg += rates[i];
        bpmAvg /= RATE_SIZE;
      }
    }
    lastBeatTime = now;  // Atualiza tempo do último batimento
  }
  irPrev = signal;  // Armazena sinal para próxima iteração
  
  // --- CÁLCULO DE SpO2 ---
  if (millis() - lastSpO2Calc > 500) {  // Calcula a cada 500ms
    
    static long irDC = 0;   // Componente DC do IR
    static long redDC = 0;  // Componente DC do vermelho
    
    // Calcula componentes DC com média móvel
    irDC  = irDC  * 0.95 + irValue  * 0.05;
    redDC = redDC * 0.95 + redValue * 0.05;
    
    // Componentes AC = Sinal total - Componente DC
    long irAC  = irValue  - irDC;
    long redAC = redValue - redDC;
    
    if (irAC > 300 && redAC > 300) {  // Verifica se sinal é forte o suficiente
      // Razão R = (AC_vermelho/DC_vermelho) / (AC_IR/DC_IR)
      float ratio = (float(redAC) / redDC) / (float(irAC) / irDC);
      // Fórmula empírica para SpO2
      int s = constrain(110 - 25 * ratio, 85, 100);
      
      // Armazena no buffer circular
      spo2Buffer[spo2Index++] = s;
      spo2Index %= SPO2_SIZE;
      
      // Calcula média dos últimos 3 valores
      int soma = 0;
      for (byte i = 0; i < SPO2_SIZE; i++) soma += spo2Buffer[i];
      spo2 = soma / SPO2_SIZE;
    }
    
    lastSpO2Calc = millis();  // Atualiza tempo do último cálculo
  }
  
  // --- LEITURA DE TEMPERATURA ---
  static unsigned long lastTemp = 0;
  if (millis() - lastTemp > 2000) {  // Lê a cada 2 segundos
    float t = readMAX30205();        // Lê sensor
    if (!isnan(t))                   // Se leitura válida
      temperatura = temperatura * 0.8 + t * 0.2;  // Filtro passa-baixa
    lastTemp = millis();
  }
  
  updateDisplay();  // Atualiza display OLED
  
  // --- ENVIO PARA FIREBASE ---
  if (sensoresProntos && millis() - lastFirebaseSend > FIREBASE_INTERVAL) {
    sendToFirebase();
    lastFirebaseSend = millis();  // Atualiza timer
  }
}
```

## FUNÇÃO updateDisplay()

```cpp
void updateDisplay() {
  oled.clearBuffer();  // Limpa buffer
  
  // Título
  oled.setFont(oled_font_5x8_tr);
  oled.setCursor(12, 8);
  oled.print("Sinais Vitais");
  
  // Status Firebase
  oled.setCursor(12, 16);
  oled.print(firebaseOnline ? "Online" : "Offline");
  
  oled.drawLine(0, 18, 127, 18);  // Linha divisória
  
  // Dados dos sensores
  oled.setFont(oled_font_6x10_tr);
  
  // BPM
  oled.setCursor(0, 32);
  oled.print("BPM: ");
  bpmAvg > 0 ? oled.print(bpmAvg) : oled.print("--");
  
  // SpO2
  oled.setCursor(0, 44);
  oled.print("SpO2: ");
  spo2 > 0 ? oled.print(spo2) : oled.print("--");
  oled.print(" %");
  
  // Temperatura
  oled.setCursor(0, 58);
  oled.print("Temp: ");
  oled.print(temperatura, 1);  // 1 casa decimal
  oled.print(" C");
  
  oled.sendBuffer();  // Envia para display
}
```


## FUNÇÃO readMAX30205()

```cpp
float readMAX30205() {
  Wire.beginTransmission(MAX30205_ADDRESS);  // Inicia comunicação com sensor
  Wire.write(0x00);  // Seleciona registrador de temperatura
  
  // Finaliza transmissão sem parar (para continuar leitura)
  if (Wire.endTransmission(false) != 0) return NAN;  // Erro na transmissão
  
  // Solicita 2 bytes de dados
  if (Wire.requestFrom(MAX30205_ADDRESS, 2) != 2) return NAN;  // Erro na leitura
  
  // Combina os 2 bytes em um inteiro de 16 bits
  int16_t raw = (Wire.read() << 8) | Wire.read();
  
  // Converte para temperatura (cada LSB = 0.00390625°C = 1/256)
  return raw * 0.00390625;
}
```

## FUNÇÃO getTimestamp()

```cpp
String getTimestamp() {
  struct tm timeinfo;  // Estrutura para armazenar data/hora
  
  // Obtém hora local (UTC-3 configurado no setup)
  if (!getLocalTime(&timeinfo)) return "sem_tempo";
  
  // Formata timestamp: "YYYY-MM-DD_HH-MM-SS"
  char buffer[25];
  strftime(buffer, sizeof(buffer), "%Y-%m-%d_%H-%M-%S", &timeinfo);
  
  return String(buffer);
}
```


## FUNÇÃO sendToFirebase()

```cpp
void sendToFirebase() {
  // Verifica se Firebase está pronto
  if (!Firebase.ready()) {
    firebaseOnline = false;
    return;
  }
  
  bool ok = true;  // Flag de sucesso
  
  // Envia dados atuais (nó "atuais")
  ok &= Firebase.RTDB.setFloat(&fbdo, "/atuais/temp", temperatura);
  ok &= Firebase.RTDB.setInt(&fbdo, "/atuais/bpm", bpmAvg);
  ok &= Firebase.RTDB.setInt(&fbdo, "/atuais/spo2", spo2);
  
  // Envia para histórico com timestamp
  String timestamp = getTimestamp();
  String path = "/historico/" + timestamp;
  
  ok &= Firebase.RTDB.setFloat(&fbdo, path + "/temp", temperatura);
  ok &= Firebase.RTDB.setInt(&fbdo, path + "/bpm", bpmAvg);
  ok &= Firebase.RTDB.setInt(&fbdo, path + "/spo2", spo2);
  
  firebaseOnline = ok;  // Atualiza status da conexão
}
```

