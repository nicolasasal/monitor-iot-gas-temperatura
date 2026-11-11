#include <DHTesp.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <WiFiClientSecure.h>

const int DHT_PIN = 15;
const int GAS_PIN = 34;
const int ALARMA_PIN = 26;
const int SWITCH_PIN = 27;

const char* WIFI_SSID = "Wokwi-GUEST";
const char* WIFI_PASS = "";

const char* MQTT_SERVER = "broker.hivemq.com";
const int   MQTT_PORT   = 8883;

const char* TOPIC_TEMP = "tensurapjr/temperatura";
const char* TOPIC_GAS  = "tensurapjr/gas";
const char* TOPIC_ALARMA_CMD = "tensurapjr/alarma/control";
const char* TOPIC_ALARMA_EST = "tensurapjr/alarma/estado";
const char* TOPIC_HIST = "tensurapjr/historial";



// LCD I2C: dirección 0x27, 16 columnas, 2 filas
LiquidCrystal_I2C lcd(0x27, 16, 2);

DHTesp dhtSensor;

WiFiClientSecure espClient;
PubSubClient mqtt(espClient);
struct Lectura {                  
  float temp;
  int gas;
};

const int MAX_BUFFER = 20;
Lectura buffer[MAX_BUFFER];
int pendientes = 0;

void conectarWiFi() {
  Serial.print("Conectando a WiFi");
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println(" WiFi conectado!");
}


void conectarMQTT() {
  while (!mqtt.connected()) {
    Serial.print("Conectando a MQTT...");
    String idCliente = "esp32-" + String(random(0xffff), HEX);
    if (mqtt.connect(idCliente.c_str())) {
      Serial.println(" MQTT conectado!");
      mqtt.subscribe(TOPIC_ALARMA_CMD);
    } else {
      Serial.print(" fallo, código ");
      Serial.println(mqtt.state());
      delay(2000);
    }
  }
}

void alRecibirMensaje(char* topic, byte* payload, unsigned int length) {
  String mensaje = "";
  for (unsigned int i = 0; i < length; i++) {
    mensaje += (char)payload[i];
  }

  Serial.print("Mensaje recibido: ");
  Serial.println(mensaje);

  if (mensaje == "ON") {
    digitalWrite(ALARMA_PIN, HIGH);
    mqtt.publish(TOPIC_ALARMA_EST, "ON");
  } else if (mensaje == "OFF") {
    digitalWrite(ALARMA_PIN, LOW);
    mqtt.publish(TOPIC_ALARMA_EST, "OFF");
  }
}

void guardarPendiente(float t, int g) {
  if (pendientes < MAX_BUFFER) {
    buffer[pendientes].temp = t;
    buffer[pendientes].gas = g;
    pendientes++;
    Serial.print("[OFFLINE] Guardado. Pendientes: ");
    Serial.println(pendientes);
  } else {
    Serial.println("[OFFLINE] Buffer lleno");
  }
}

void enviarPendientes() {
  if (pendientes == 0) return;

  for (int i = 0; i < pendientes; i++) {
    String msg = String(buffer[i].temp, 1) + "," + String(buffer[i].gas);
    mqtt.publish(TOPIC_HIST, msg.c_str());
  }
  Serial.println("[SYNC] Buffer enviado");
  pendientes = 0;
}

void setup() {

  Serial.begin(115200);

  // Iniciar DHT22
  dhtSensor.setup(DHT_PIN, DHTesp::DHT22);
  pinMode(SWITCH_PIN, INPUT_PULLUP);

  // Iniciar I2C
  Wire.begin(21, 22);

  pinMode(ALARMA_PIN, OUTPUT);
  digitalWrite(ALARMA_PIN, LOW);
  mqtt.setCallback(alRecibirMensaje);

  // Iniciar LCD
  lcd.init();
  lcd.backlight();

  // Mensaje inicial
  lcd.setCursor(0, 0);
  lcd.print("Sistema iniciado");

  Serial.println("Sistema iniciado");

  delay(2000);

  lcd.clear();
  conectarWiFi();
  espClient.setInsecure();
  mqtt.setServer(MQTT_SERVER, MQTT_PORT);
}

void loop() {

  bool cortado = (digitalRead(SWITCH_PIN) == LOW);

  if (cortado) {
    if (mqtt.connected()) mqtt.disconnect();
    WiFi.disconnect();
    Serial.println("[OFFLINE] Sin conexion");
  } else {
    if (WiFi.status() != WL_CONNECTED) conectarWiFi();
    if (!mqtt.connected()) conectarMQTT();
    mqtt.loop();
  }

  // Leer DHT22
  TempAndHumidity data = dhtSensor.getTempAndHumidity();

  // Leer MQ2
  int gasValue = analogRead(GAS_PIN);

  // =========================
  // MONITOR SERIAL
  // =========================

  Serial.println("------ DATOS ------");

  Serial.print("Temperatura: ");
  Serial.print(data.temperature);
  Serial.println(" C");

  Serial.print("Gas: ");
  Serial.println(gasValue);

  Serial.println("-------------------");

    if (mqtt.connected()) {
    enviarPendientes();
    mqtt.publish(TOPIC_TEMP, String(data.temperature, 1).c_str());
    mqtt.publish(TOPIC_GAS,  String(gasValue).c_str());
  } else {
    guardarPendiente(data.temperature, gasValue);
  }
  // =========================
  // LCD
  // =========================

  lcd.clear();

  lcd.setCursor(0, 0);
  lcd.print("Temp: ");
  lcd.print(data.temperature, 1);
  lcd.print((char)223);
  lcd.print("C");

  lcd.setCursor(0, 1);
  lcd.print("Gas: ");
  lcd.print(gasValue);

  delay(2000);
}
