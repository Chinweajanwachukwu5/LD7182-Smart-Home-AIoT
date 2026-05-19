/*
 * Smart Home AIoT Monitoring System
 * Module: LD7182 - AI for IoT
 * Hardware: ESP32-S3 + DHT22 + MQ2 + PIR + LDR + HC-SR04 + OLED
 */

#include <DHT.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// Pin definitions
#define DHTPIN    15
#define DHTTYPE   DHT22
#define PIR_PIN   5
#define MQ2_PIN   1
#define LDR_PIN   2
#define TRIG_PIN  9
#define ECHO_PIN  10
#define BUZZER    12
#define LED       13
#define SDA_PIN   8
#define SCL_PIN   35

// WiFi and ThingSpeak
const char* ssid     = "Wokwi-GUEST";
const char* password = "";
const char* apiKey   = "81TG8YL4LIYULML7";

// OLED
Adafruit_SSD1306 display(128, 64, &Wire, -1);
DHT dht(DHTPIN, DHTTYPE);
WiFiClientSecure client;

unsigned long lastSendTime = 0;
const unsigned long sendInterval = 20000;

// Function prototypes
String determineSystemState(float temp, int gas, int motion, int ldr, long dist);
void handleActuators(String state);
void updateDisplay(float temp, float hum, int gas, int ldr, long dist, String state);
void sendToThingSpeak(float temp, float hum, int gas, int motion, String state);

void setup() {
  Serial.begin(115200);
  delay(1000);
  dht.begin();
  delay(2000);

  pinMode(PIR_PIN, INPUT);
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  pinMode(BUZZER, OUTPUT);
  pinMode(LED, OUTPUT);
  digitalWrite(BUZZER, LOW);
  digitalWrite(LED, LOW);

  Wire.begin(SDA_PIN, SCL_PIN);
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("OLED FAILED");
    while (true);
  }
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(15, 20);
  display.println("Smart Home AIoT");
  display.setCursor(30, 35);
  display.println("Starting...");
  display.display();

  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi Connected!");
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());
  client.setInsecure();

  display.clearDisplay();
  display.setCursor(10, 25);
  display.println("WiFi Connected!");
  display.display();
  delay(1500);
}

void loop() {
  // Phase 1: Data Acquisition
  float temp   = dht.readTemperature();
  float hum    = dht.readHumidity();
  int   gas    = analogRead(MQ2_PIN);
  int   ldr    = analogRead(LDR_PIN);
  int   motion = digitalRead(PIR_PIN);

  if (isnan(temp) || isnan(hum)) {
    Serial.println("DHT ERROR - retrying...");
    delay(3000);
    return;
  }

  // Ultrasonic distance
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);
  long duration = pulseIn(ECHO_PIN, HIGH, 30000);
  long dist     = duration * 0.034 / 2;

  // Phase 2: Intelligence
  String state = determineSystemState(temp, gas, motion, ldr, dist);

  // Phase 3: Actuation
  handleActuators(state);

  // Phase 4: Reporting
  updateDisplay(temp, hum, gas, ldr, dist, state);

  Serial.println("-----------------------------");
  Serial.print("Temp: ");     Serial.print(temp);   Serial.println(" C");
  Serial.print("Humidity: "); Serial.print(hum);    Serial.println(" %");
  Serial.print("Gas: ");      Serial.println(gas);
  Serial.print("LDR: ");      Serial.println(ldr);
  Serial.print("Distance: "); Serial.print(dist);   Serial.println(" cm");
  Serial.print("Motion: ");   Serial.println(motion);
  Serial.print("State: ");    Serial.println(state);

  if (millis() - lastSendTime >= sendInterval) {
    sendToThingSpeak(temp, hum, gas, motion, state);
    lastSendTime = millis();
  }
  delay(2000);
}

String determineSystemState(float temp, int gas, int motion, int ldr, long dist) {
  if (temp > 70)                              return "OVERHEATING";
  if (gas > 2000)                             return "GAS ALERT";
  if (gas > 1500 && motion == 0)              return "POSSIBLE LEAK";
  if (gas > 1500 && motion == 1)              return "GAS WARNING";
  if (ldr < 1000 && motion == 0)              return "LIGHTS ON EMPTY";
  if (dist > 0 && dist < 50 && motion == 0)   return "PRESENCE DETECTED";
  if (motion == 1)                            return "NORMAL";
  return "UNOCCUPIED";
}

void handleActuators(String state) {
  if (state == "GAS ALERT" ||
      state == "POSSIBLE LEAK" ||
      state == "OVERHEATING") {
    digitalWrite(BUZZER, HIGH);
    digitalWrite(LED, HIGH);
  } else {
    digitalWrite(BUZZER, LOW);
    digitalWrite(LED, LOW);
  }
}

void updateDisplay(float temp, float hum, int gas, int ldr, long dist, String state) {
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println("Smart Home AIoT");
  display.drawLine(0, 10, 127, 10, SSD1306_WHITE);
  display.setCursor(0, 15);
  display.print("T:"); display.print(temp, 1); display.print("C ");
  display.print("H:"); display.print(hum, 1);  display.println("%");
  display.setCursor(0, 28);
  display.print("Gas:"); display.println(gas);
  display.setCursor(0, 40);
  display.print("Dist:"); display.print(dist); display.println("cm");
  display.setCursor(0, 52);
  if (state.length() > 20) {
    display.println(state.substring(0, 20));
  } else {
    display.println(state);
  }
  display.display();
}

void sendToThingSpeak(float temp, float hum, int gas, int motion, String state) {
  if (client.connect("api.thingspeak.com", 443)) {
    int stateCode = 0;
    if      (state == "NORMAL")             stateCode = 1;
    else if (state == "UNOCCUPIED")         stateCode = 2;
    else if (state == "GAS WARNING")        stateCode = 3;
    else if (state == "POSSIBLE LEAK")      stateCode = 4;
    else if (state == "GAS ALERT")          stateCode = 5;
    else if (state == "OVERHEATING")        stateCode = 6;
    else if (state == "LIGHTS ON EMPTY")    stateCode = 7;
    else if (state == "PRESENCE DETECTED")  stateCode = 8;

    String postStr = "api_key=" + String(apiKey);
    postStr += "&field1=" + String(temp);
    postStr += "&field2=" + String(hum);
    postStr += "&field3=" + String(gas);
    postStr += "&field4=" + String(motion);
    postStr += "&field5=" + String(stateCode);

    client.println("POST /update HTTP/1.1");
    client.println("Host: api.thingspeak.com");
    client.println("Connection: close");
    client.println("Content-Type: application/x-www-form-urlencoded");
    client.println("Content-Length: " + String(postStr.length()));
    client.println();
    client.print(postStr);
    delay(1000);
    Serial.println("Data sent to ThingSpeak!");
    client.stop();
  } else {
    Serial.println("ThingSpeak connection failed.");
  }
}
