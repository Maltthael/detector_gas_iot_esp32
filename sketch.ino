#include <WiFi.h>
#include <PubSubClient.h>
#include <ESP32Servo.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// Rede e MQTT
const char* SSID = "Wokwi-GUEST";
const char* PASSWORD = "";
const char* MQTT_SERVER = "broker.hivemq.com";
const int MQTT_PORT = 1883;

const char* TOPICO_TELEMETRIA = "projeto/cozinha/detector_gas";
const char* TOPICO_COMANDO    = "projeto/cozinha/comando";

// Pinos
#define PINO_SENSOR_GAS 34
#define PINO_BUZZER     25
#define PINO_LED_PERIGO 26
#define PINO_LED_EXAUST 27
#define PINO_SERVO      18
#define PINO_BOTAO_RSET 14

// Calibração do Sensor MQ-2 
const int LEITURA_MIN = 843;
const int LEITURA_MAX = 4041;
const int LIMITE_ATENCAO = 35; // em %
const int LIMITE_PERIGO  = 70; // em %

WiFiClient espClient;
PubSubClient client(espClient);
Servo valvulaGas;
Adafruit_SSD1306 display(128, 64, &Wire, -1);

// Variáveis de Estado
int leituraBruta = 0;
int percentualGas = 0;
bool valvulaFechada = false;
bool alarmeSilenciado = false;
String statusCozinha = "SEGURO";
unsigned long ultimoEnvioMQTT = 0;

void setup() {
  Serial.begin(115200);
  
  pinMode(PINO_SENSOR_GAS, INPUT);
  pinMode(PINO_BUZZER, OUTPUT);
  pinMode(PINO_LED_PERIGO, OUTPUT);
  pinMode(PINO_LED_EXAUST, OUTPUT);
  pinMode(PINO_BOTAO_RSET, INPUT_PULLUP);

  valvulaGas.attach(PINO_SERVO);
  valvulaGas.write(0); // 0 graus = Válvula Aberta

  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("Falha no OLED");
  }
  display.clearDisplay();
  display.setTextColor(WHITE);

  conectarWiFi();
  client.setServer(MQTT_SERVER, MQTT_PORT);
  client.setCallback(receberComandoMQTT);
}

void loop() {
  if (!client.connected()) {
    reconectarMQTT();
  }
  client.loop();

  verificarBotaoRearme();
  lerSensorGas();
  avaliarSeguranca();
  atualizarDisplayLocal();

  // Publica a cada 2 segundos sem travar o loop (usando millis)
  if (millis() - ultimoEnvioMQTT >= 2000) {
    ultimoEnvioMQTT = millis();
    publicarDadosMQTT();
  }
}

void conectarWiFi() {
  WiFi.begin(SSID, PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(300);
  }
}

void reconectarMQTT() {
  while (!client.connected()) {
    String clientId = "ESP32_Gas_Pro_" + String(random(0xffff), HEX);
    if (client.connect(clientId.c_str())) {
      client.subscribe(TOPICO_COMANDO); // Escuta comandos remotos
      Serial.println("MQTT Conectado e inscrito em comandos!");
    } else {
      delay(2000);
    }
  }
}

// Callback executado quando chega comando do painel MQTT
void receberComandoMQTT(char* topic, byte* payload, unsigned int length) {
  String mensagem = "";
  for (unsigned int i = 0; i < length; i++) {
    mensagem += (char)payload[i];
  }
  mensagem.trim();
  Serial.println("Comando remoto recebido: " + mensagem);

  if (mensagem == "FECHAR") {
    valvulaFechada = true;
    valvulaGas.write(90);
  } else if (mensagem == "REARMAR" && percentualGas < LIMITE_ATENCAO) {
    valvulaFechada = false;
    alarmeSilenciado = false;
    valvulaGas.write(0);
  } else if (mensagem == "SILENCIAR") {
    alarmeSilenciado = true;
    noTone(PINO_BUZZER);
  }
}

void verificarBotaoRearme() {
  if (digitalRead(PINO_BOTAO_RSET) == LOW) {
    if (percentualGas < LIMITE_ATENCAO) {
      valvulaFechada = false;
      alarmeSilenciado = false;
      valvulaGas.write(0);
      Serial.println("Sistema rearmado manualmente!");
    } else {
      Serial.println("Bloqueado: Nivel de gas ainda alto para rearmar!");
    }
    delay(300);
  }
}

void lerSensorGas() {
  leituraBruta = analogRead(PINO_SENSOR_GAS);
  percentualGas = map(leituraBruta, LEITURA_MIN, LEITURA_MAX, 0, 100);
  percentualGas = constrain(percentualGas, 0, 100);
}

void avaliarSeguranca() {
  if (percentualGas >= LIMITE_PERIGO) {
    statusCozinha = "PERIGO";
    valvulaFechada = true;
    valvulaGas.write(90); // Fecha registro de gás (90 graus)
    digitalWrite(PINO_LED_PERIGO, HIGH);
    digitalWrite(PINO_LED_EXAUST, HIGH); // Liga exaustor
    if (!alarmeSilenciado) {
      tone(PINO_BUZZER, 2000);
    }
  } 
  else if (percentualGas >= LIMITE_ATENCAO) {
    statusCozinha = "ATENCAO";
    digitalWrite(PINO_LED_EXAUST, HIGH); // Liga exaustor preventivamente
    digitalWrite(PINO_LED_PERIGO, (millis() / 500) % 2); // Pisca sem delay
    if (!alarmeSilenciado) {
      // Bip suave de 120ms a cada 2 segundos
      if ((millis() % 2000) < 120) tone(PINO_BUZZER, 600);
      else noTone(PINO_BUZZER);
    }
  } 
  else {
    statusCozinha = "SEGURO";
    alarmeSilenciado = false;
    noTone(PINO_BUZZER);
    digitalWrite(PINO_LED_PERIGO, LOW);
    digitalWrite(PINO_LED_EXAUST, LOW);
    // Nota: Se a válvula fechou por perigo, ela permanece fechada até o rearme!
  }
}

void atualizarDisplayLocal() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.print("MONITOR GAS IoT");
  display.drawLine(0, 10, 128, 10, WHITE);

  display.setCursor(0, 15);
  display.print("Nivel: ");
  display.setTextSize(2);
  display.print(percentualGas);
  display.println("%");

  display.setTextSize(1);
  display.setCursor(0, 36);
  display.print("Status: ");
  display.println(statusCozinha);

  display.setCursor(0, 48);
  display.print("Valvula: ");
  display.println(valvulaFechada ? "FECHADA (90)" : "ABERTA (0)");

  // Barra de progresso visual na base
  int larguraBarra = map(percentualGas, 0, 100, 0, 128);
  display.fillRect(0, 59, larguraBarra, 5, WHITE);
  display.display();
}

void publicarDadosMQTT() {
  String payload = "{";
  payload += "\"gas_pct\":" + String(percentualGas) + ",";
  payload += "\"status\":\"" + statusCozinha + "\",";
  payload += "\"valvula\":\"" + String(valvulaFechada ? "FECHADA" : "ABERTA") + "\",";
  payload += "\"exaustor\":" + String(digitalRead(PINO_LED_EXAUST) ? "true" : "false");
  payload += "}";

  client.publish(TOPICO_TELEMETRIA, payload.c_str());
  Serial.println("Telemetria: " + payload);
}