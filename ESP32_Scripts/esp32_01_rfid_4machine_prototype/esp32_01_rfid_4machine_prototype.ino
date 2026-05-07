#include <WiFi.h>
#include <HTTPClient.h>
#include <SPI.h>
#include <MFRC522.h>
#include <ArduinoJson.h>

// WiFi
#define WIFI_SSID "SLT-Fiber-BHPf8-2.4G_EXT"
#define WIFI_PASSWORD "Bluetex@5724"

// API
#define VERIFY_URL "http://192.168.1.185:3000/verify-rfid"
#define LOG_URL "http://192.168.1.185:3000/machine-done"

// ESP ID
#define ESP_ID "ESP32_1"

// RFID
#define SS_PIN 5
#define RST_PIN 22

// Output
#define GREEN_LED 12
#define BUZZER 13

// Buttons (Start + End)
int startButtons[4] = {14, 25, 27, 33};
int endButtons[4]   = {15, 26, 32, 34};

bool startState[4];
bool endState[4];

MFRC522 rfid(SS_PIN, RST_PIN);

bool employeeVerified = false;
int employeeId = 0;
String currentUID = "";

// ---------------- SETUP ----------------
void setup() {
  Serial.begin(115200);

  // Buttons
  for (int i = 0; i < 4; i++) {
    pinMode(startButtons[i], INPUT_PULLUP);
    pinMode(endButtons[i], (endButtons[i] == 34) ? INPUT : INPUT_PULLUP);

    startState[i] = HIGH;
    endState[i] = HIGH;
  }

  pinMode(GREEN_LED, OUTPUT);
  pinMode(BUZZER, OUTPUT);

  digitalWrite(GREEN_LED, LOW);
  digitalWrite(BUZZER, LOW);

  SPI.begin(18, 19, 23, SS_PIN);
  rfid.PCD_Init();

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting WiFi");

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nReady - Scan RFID");
}

// ---------------- UTIL ----------------
void beep() {
  digitalWrite(BUZZER, HIGH);
  delay(150);
  digitalWrite(BUZZER, LOW);
}

String readUID() {
  String uid = "";
  for (byte i = 0; i < rfid.uid.size; i++) {
    uid += String(rfid.uid.uidByte[i], HEX);
    if (i < rfid.uid.size - 1) uid += " ";
  }
  uid.toUpperCase();
  return uid;
}

// ---------------- RFID VERIFY ----------------
void verifyRFID(String uid) {
  HTTPClient http;
  http.begin(VERIFY_URL);
  http.addHeader("Content-Type", "application/json");

  String body = "{\"rfid_uid\":\"" + uid + "\"}";
  int res = http.POST(body);
  String payload = http.getString();

  Serial.println("RFID: " + uid);
  Serial.println(payload);

  if (res == 200) {
    DynamicJsonDocument doc(512);
    deserializeJson(doc, payload);

    if (doc["success"]) {
      employeeVerified = true;
      employeeId = doc["employee_id"];
      currentUID = uid;

      digitalWrite(GREEN_LED, HIGH);
      beep();

      Serial.println("Employee verified");
    } else {
      employeeVerified = false;
      digitalWrite(GREEN_LED, LOW);
      Serial.println("Invalid card");
    }
  }

  http.end();
}

// ---------------- SEND LOG ----------------
void sendLog(int machineNo, String action) {
  if (!employeeVerified) {
    Serial.println("Scan RFID first!");
    return;
  }

  HTTPClient http;
  http.begin(LOG_URL);
  http.addHeader("Content-Type", "application/json");

  String json = "{";
  json += "\"esp_id\":\"" + String(ESP_ID) + "\",";
  json += "\"employee_id\":" + String(employeeId) + ",";
  json += "\"rfid_uid\":\"" + currentUID + "\",";
  json += "\"machine\":" + String(machineNo) + ",";
  json += "\"action\":\"" + action + "\"";
  json += "}";

  int res = http.POST(json);

  Serial.println(json);
  Serial.println(res);

  http.end();

  if (res > 0) {
    beep();
    digitalWrite(GREEN_LED, LOW);

    employeeVerified = false;
    employeeId = 0;
    currentUID = "";

    Serial.println("Done. Scan again.");
  }
}

// ---------------- LOOP ----------------
void loop() {

  // RFID
  if (rfid.PICC_IsNewCardPresent() && rfid.PICC_ReadCardSerial()) {
    String uid = readUID();
    verifyRFID(uid);
    rfid.PICC_HaltA();
    delay(500);
  }

  // Buttons
  for (int i = 0; i < 4; i++) {

    // START button
    int s = digitalRead(startButtons[i]);
    if (s == LOW && startState[i] == HIGH) {
      Serial.print("Machine ");
      Serial.print(i + 1);
      Serial.println(" START");

      sendLog(i + 1, "start");
      startState[i] = LOW;
    }
    if (s == HIGH) startState[i] = HIGH;

    // END button
    int e = digitalRead(endButtons[i]);
    if (e == LOW && endState[i] == HIGH) {
      Serial.print("Machine ");
      Serial.print(i + 1);
      Serial.println(" END");

      sendLog(i + 1, "end");
      endState[i] = LOW;
    }
    if (e == HIGH) endState[i] = HIGH;
  }

  delay(50);
}