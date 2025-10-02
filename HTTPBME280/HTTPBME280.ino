/*
* Conexion con el sensor BME280
*/

#include <WiFi.h>
#include <ESP32Time.h>
#include <HTTPClient.h>
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>
#include <time.h>
#include <sys/time.h>

#define ID_BME 0x76
#define INTERVALO 60000
#define BUILTIN_LED 2
#define MSG_BUFFER_SIZE  (300)

// Setup to connect ESP32 to WiFi and MQTT Server
const char* WIFI_SSID = <YOUR SSID>;
const char* WIFI_PASSWORD = <YOUR PASSWORD>;
const char* IP_SERVER = <YOUR SERVER IP>; // Ej: "tb.mi-dominio.com" o "IP:PUERTO" (192.168.2.10:8080)
const char* ACCESS_TOKEN = <YOUR DEVICE ACCESS TOKEN>; // DEVICE ACCESS TOKEN

// BME280 Setup
Adafruit_BME280 bme; // I2C

//Setuo to get the actual time
ESP32Time rtc;
const char* NTPSERVER = "pool.ntp.org";
const long  GMTOFFSET_SEC = 0;
const int   DAYLIGHTOFFSET_SEC = 0;

// Tiempo de medicion
unsigned long now = 0;
unsigned long lastMsgTime = 0;

//Initialization WiFi Client
WiFiClient espClient;

unsigned long lastMsg = 0;

void setup() {
  pinMode(BUILTIN_LED, OUTPUT);     // Initialize the BUILTIN_LED pin as an output
  Serial.begin(115200);
  setupWifi();
  configTime(GMTOFFSET_SEC, DAYLIGHTOFFSET_SEC, NTPSERVER);
  initRtc();

  setupBME();
  digitalWrite(BUILTIN_LED, HIGH);
  now = millis();
  sendDataBME();
  lastMsgTime = now;
}


void loop() { 
  now = millis();
  if(now >= (lastMsgTime + INTERVALO)) {
    sendDataBME();
    lastMsgTime = now;
  }
}

void setupWifi() {

  delay(10);
  // We start by connecting to a WiFi network
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(WIFI_SSID);

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  randomSeed(micros());

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}


void initRtc() {
  struct tm timeinfo;
  if (getLocalTime(&timeinfo)) {
    rtc.setTimeStruct(timeinfo);
  }
}


unsigned long long getEpochMillis() {
  struct timespec ts;
  clock_gettime(CLOCK_REALTIME, &ts);
  return (unsigned long long)ts.tv_sec * 1000ULL + ts.tv_nsec / 1000000ULL;
}

void setupBME() {
  bool status_BME;
  status_BME = bme.begin(ID_BME);  
  if (!status_BME) {
    Serial.println("Could not find a valid BME280 sensor, check wiring!");
    return;
  }
}


String tbBuildTelemetryUrl(const String& host, const String& deviceToken, bool https = false, uint16_t port = 0) {
  // String url = https ? "https://" : "http://";
  String url = "http://";
  url += host;
  if (port != 0) url += ":" + String(port);
  url += "/api/v1/" + deviceToken + "/telemetry";
  Serial.println(url);
  return url;
}

/**
 * Envía telemetría a ThingsBoard.
 *
 * @param host         Ej: "tb.mi-dominio.com" o "192.168.1.50"
 * @param deviceToken  Token del dispositivo en TB
 * @param bodyJson     Cuerpo JSON (ver ejemplos abajo)
 * @param https        true para HTTPS
 * @param port         0 = por defecto (80/443). O especifica ej. 8080/8443
 * @param timeoutMs    timeout de la petición
 * @param insecureTLS  true -> http.setInsecure() (solo pruebas)
 * @param responseOut  (opcional) respuesta texto del server (suele venir vacía)
 * @return             Código HTTP (200=OK, 204=OK sin cuerpo). <0 si error cliente.
 */
int tbPostTelemetry(
  const String& host,
  const String& deviceToken,
  const String& bodyJson,
  bool https = false,
  uint16_t port = 0,
  uint16_t timeoutMs = 5000,
  bool insecureTLS = true,
  String* responseOut = nullptr
) {
  if (WiFi.status() != WL_CONNECTED) return -1;

  String url = tbBuildTelemetryUrl(host, deviceToken, https, port);

  HTTPClient http;
  http.setTimeout(timeoutMs);

  if (!http.begin(url)) return -2;

  http.addHeader("Content-Type", "application/json");
  int code = http.POST((uint8_t*)bodyJson.c_str(), bodyJson.length());

  if (code > 0 && responseOut) {
    *responseOut = http.getString();  // en TB suele estar vacía
  }

  http.end();
  return code;
}

void sendDataBME() {

  float temperature = bme.readTemperature();
  float humidity = bme.readHumidity();
  float pressure = bme.readPressure() / 100.0F;
  unsigned long long tsMicros = getEpochMillis();

  //Format message to influx-format
  char msg[MSG_BUFFER_SIZE]; //Message to send to Broker
  snprintf(msg, MSG_BUFFER_SIZE, "{\"ts\":%llu, \"values\":{\"temperature\":%.2f,\"relative_humidity\":%.2f,\"atmospheric_pressure\":%.2f}}", tsMicros, temperature, humidity, pressure);
  Serial.print("Publish message: ");
  Serial.println(msg);

  int code = tbPostTelemetry(/*host=*/IP_SERVER, /*deviceToken=*/ACCESS_TOKEN, /*bodyJson=*/msg);
  Serial.printf("TB code: %d\n", code);  // normalmente 200 o 204
  if (code == 200 || code == 204) {
    digitalWrite(BUILTIN_LED, LOW);
    delay(500);
  }
  digitalWrite(BUILTIN_LED, HIGH);
}
