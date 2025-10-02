/*
* Conexion con el sensor BME280
*/

#include <WiFi.h>
#include <ESP32Time.h>
#include <PubSubClient.h> //Documentation: https://github.com/knolleary/pubsubclient
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
const char* IP_BROKER = <YOUR BROKER IP>;
const int PORT = <YOUR BROKER PORT>;
const char* MQTT_USER = <YOUR DEVICE ACCESS TOKEN>; // DEVICE ACCESS TOKEN
const char* MQTT_PASSWORD = "";
const char* DATA_TOPIC = "v1/devices/me/telemetry";
String CLIENT_ID = "ESP32Client-" + String(random(0xffff), HEX);

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

//Initialization WiFi Client and MQTT Client
WiFiClient espClient;
PubSubClient client(espClient);

unsigned long lastMsg = 0;

void setup() {
  pinMode(BUILTIN_LED, OUTPUT);     // Initialize the BUILTIN_LED pin as an output
  Serial.begin(115200);
  setupWifi();
  configTime(GMTOFFSET_SEC, DAYLIGHTOFFSET_SEC, NTPSERVER);
  initRtc();
  client.setServer(IP_BROKER, PORT);
  client.setCallback(callback);
  client.setBufferSize(512);
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


void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();

  // Switch on the LED if an 1 was received as first character
  if ((char)payload[0] == '1') {
    digitalWrite(BUILTIN_LED, LOW);   // Turn the LED on (Note that LOW is the voltage level
  } else {
    digitalWrite(BUILTIN_LED, HIGH);  // Turn the LED off by making the voltage HIGH
  }

}

void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (client.connect(CLIENT_ID.c_str(), MQTT_USER, MQTT_PASSWORD)) {
      Serial.println("connected");
    } else {
      Serial.print("failed, rc=");
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
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

void sendDataBME() {

  float temperature = bme.readTemperature();
  float humidity = bme.readHumidity();
  float pressure = bme.readPressure() / 100.0F;
  unsigned long long tsMicros = getEpochMillis();

  //Format message to influx-format
  char msg[MSG_BUFFER_SIZE]; //Message to send to Broker
  snprintf(msg, MSG_BUFFER_SIZE, "{\"ts\":%llu, \"values\":{\"temperature\":%.2f,\"relative_humidity"":%.2f,\"atmospheric_pressure\":%.2f}}", tsMicros, temperature, humidity, pressure);
  Serial.print("Publish message: ");
  Serial.println(msg);

  if (client.connect(CLIENT_ID.c_str(), MQTT_USER, MQTT_PASSWORD)) {
      client.publish(DATA_TOPIC, msg);
      digitalWrite(BUILTIN_LED, LOW);
      delay(500);
    }
  digitalWrite(BUILTIN_LED, HIGH);
}
