#include <ESP32Servo.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <WiFi.h>
#include <WebServer.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>

// ================= CONFIGURATION =================
const char* ssid = "Kong_Wifi";
const char* password = "0651166902z";
const char* discordWebhookUrl = "https://discord.com/api/webhooks/1462461338302546014/1SP7qRd3Q4aKRl5Ql9yjapL_TAA0JO8D1eib5AG1IgSg9MsyJtm2_mzTFYB-lLq1acdx"; 
const char* googleApiKey = "AIzaSyDBRn26Zx2CLXtLI-0MsmPs1poAwEJvROQ";

const int THRESHOLD = 600;  
const int HYSTERESIS = 100;

// ================= PIN DEFINITIONS =================
#define SERVO_PIN 19      
#define MQ2_PIN 36        
#define BUTTON_PIN 27     
#define RELAY_PIN 18      
#define BUZZER_PIN 5//14
#define LED_R 16
#define LED_Y 17
#define LED_G 15

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

Servo myServo;
WebServer server(80);

const int POS_LOCKED = 90;
const int POS_UNLOCKED = 180;

// ================= VARIABLES =================
bool isLocked = true;
bool isSmokeDetected = false;
bool lastLockedState = !isLocked;
bool discordSent = false;
bool isTestMode = false; 
volatile int sharedSmokeValue = 0; 

String latitude = "";
String longitude = "";
float accuracy = 0;

unsigned long lastDisplayTime = 0;
unsigned long lastBuzzerTime = 0;
unsigned long buttonPressedTime = 0; 
unsigned long lastLEDTime = 0;
int ledStep = 0;
bool isButtonPressed = false;
bool resetHandled = false; 

TaskHandle_t networkTaskHandle;

// ================= NETWORK TASK (CORE 0) =================
// ทำงานแยกอิสระเพื่อไม่ให้ Main Loop สะดุด
void getGoogleLocation() {
  int n = WiFi.scanNetworks(false, true, false, 300);
  if (n == 0) return;

  DynamicJsonDocument doc(4096);
  JsonObject root = doc.to<JsonObject>();
  JsonArray wifiArray = root.createNestedArray("wifiAccessPoints");

  for (int i = 0; i < n && i < 10; ++i) { 
    JsonObject wifiObject = wifiArray.createNestedObject();
    wifiObject["macAddress"] = WiFi.BSSIDstr(i);
    wifiObject["signalStrength"] = WiFi.RSSI(i);
  }

  String requestBody;
  serializeJson(doc, requestBody);
  WiFi.scanDelete(); 

  HTTPClient http;
  WiFiClientSecure client;
  client.setInsecure();
  
  if (http.begin(client, "https://www.googleapis.com/geolocation/v1/geolocate?key=" + String(googleApiKey))) {
    http.addHeader("Content-Type", "application/json");
    if (http.POST(requestBody) == 200) {
      DynamicJsonDocument responseDoc(1024);
      deserializeJson(responseDoc, http.getString());
      if (responseDoc.containsKey("location")) {
        latitude = responseDoc["location"]["lat"].as<String>();
        longitude = responseDoc["location"]["lng"].as<String>();
        accuracy = responseDoc["accuracy"].as<float>();
      }
    }
    http.end();
  }
}

void processNetworkAlert(void * parameter) {
  for(;;) {
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY); // รอคำสั่ง (Blocking)
    
    if (WiFi.status() == WL_CONNECTED) {
      getGoogleLocation();
      
      HTTPClient http; 
      WiFiClientSecure client; 
      client.setInsecure();
      
      if (http.begin(client, discordWebhookUrl)) {
        http.addHeader("Content-Type", "application/json");
        String discordContent = "🔥 **FIRE ALERT!** 🔥\\nSmoke: " + String(sharedSmokeValue);
        
        if (latitude != "") discordContent += "\\n📍 Map: http://maps.google.com/?q=" + latitude + "," + longitude;
        else discordContent += "\\n📍 Map: Locating...";

        http.POST("{\"content\": \"" + discordContent + "\"}");
        http.end();
      }
    }
  }
}

// ================= WEB SERVER =================
String getHTML() {
  String html = "<!DOCTYPE html><html><head><meta name='viewport' content='width=device-width, initial-scale=1'><meta charset='UTF-8'><meta http-equiv='refresh' content='3'>"; 
  html += "<style>body{font-family:sans-serif;text-align:center;background:#222;color:#eee}.btn{padding:15px;margin:10px;width:80%;border-radius:8px;border:none;color:white;cursor:pointer}.unlock{background:#27ae60}.test{background:#e67e22}.box{border:1px solid #444;padding:15px;margin:10px auto;width:90%;border-radius:8px;background:#333}</style></head><body>";
  html += "<h2>Lotus Security System</h2><div class='box'>";
  html += "Door: " + String(isLocked ? "<b style='color:#e74c3c'>LOCKED</b>" : "<b style='color:#2ecc71'>UNLOCKED</b>") + "<br>";
  html += "Smoke: " + String(isSmokeDetected ? "<b style='color:#e74c3c'>DETECTED!</b>" : "<b style='color:#2ecc71'>NORMAL</b>") + "</div>";
  html += "<a href='/unlock'><button class='btn unlock'>RESET / UNLOCK</button></a>";
  html += "<a href='/test'><button class='btn test'>TEST FIRE ALARM</button></a></body></html>";
  return html;
}

void handleRoot() { server.send(200, "text/html", getHTML()); }
void handleUnlock() {
  isTestMode = false;
  if (analogRead(MQ2_PIN) < THRESHOLD) {
    isSmokeDetected = false; isLocked = false; discordSent = false;
    digitalWrite(RELAY_PIN, LOW);
  }
  server.sendHeader("Location", "/"); server.send(303);
}
void handleTest() { isTestMode = true; isSmokeDetected = false; discordSent = false; server.sendHeader("Location", "/"); server.send(303); }

// ================= HARDWARE HELPERS =================
void updateLEDs() {
  if (isSmokeDetected) {
    if (millis() - lastLEDTime > 150) {
      lastLEDTime = millis();
      digitalWrite(LED_R, LOW); digitalWrite(LED_Y, LOW); digitalWrite(LED_G, LOW);
      if (ledStep == 0) digitalWrite(LED_R, HIGH);
      else if (ledStep == 1) digitalWrite(LED_Y, HIGH);
      else if (ledStep == 2) digitalWrite(LED_G, HIGH);
      else if (ledStep == 3) digitalWrite(LED_G, HIGH);
      else if (ledStep == 4) digitalWrite(LED_Y, HIGH);
      else if (ledStep == 5) digitalWrite(LED_R, HIGH);
      ledStep = (ledStep + 1) % 6;
    }
  } else {
    digitalWrite(LED_R, LOW); digitalWrite(LED_Y, LOW); digitalWrite(LED_G, LOW);
    ledStep = 0;
  }
}

void updateDisplay(int smokeVal) {
  display.clearDisplay(); display.setTextSize(1); display.setTextColor(WHITE); display.setCursor(0, 0);
  display.println(WiFi.status() == WL_CONNECTED ? WiFi.localIP().toString() : "WiFi Offline");
  display.drawLine(0, 10, 128, 10, WHITE);
  display.setCursor(10, 30);
  if (isSmokeDetected) { display.setTextColor(BLACK, WHITE); display.println("FIRE! PUMP ON"); }
  else { display.setTextColor(WHITE); display.setTextSize(2); display.println(isLocked ? " LOCKED" : " UNLOCKED"); }
  display.setTextSize(1); display.setTextColor(WHITE); display.setCursor(0, 55); display.printf("Val:%d", smokeVal); display.display();
}

// ================= MAIN SETUP & LOOP =================
void setup() {
  Serial.begin(115200);
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) delay(500);

  server.on("/", handleRoot); server.on("/unlock", handleUnlock); server.on("/test", handleTest); server.begin();
  
  ESP32PWM::allocateTimer(0); myServo.setPeriodHertz(50); myServo.attach(SERVO_PIN, 500, 2400); 
  pinMode(MQ2_PIN, INPUT); pinMode(BUZZER_PIN, OUTPUT); pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(RELAY_PIN, OUTPUT); pinMode(LED_R, OUTPUT); pinMode(LED_Y, OUTPUT); pinMode(LED_G, OUTPUT);
  
  digitalWrite(RELAY_PIN, LOW); 
  myServo.write(POS_LOCKED);

  // สร้าง Task Core 0
  xTaskCreatePinnedToCore(processNetworkAlert, "NetworkTask", 10000, NULL, 0, &networkTaskHandle, 0);
}

void loop() {
  server.handleClient();
  int sensorRead = analogRead(MQ2_PIN);
  int smokeValue = isTestMode ? 3000 : sensorRead;
  sharedSmokeValue = smokeValue;
  
  if (smokeValue > THRESHOLD) {
    if (!isSmokeDetected) {
      isSmokeDetected = true; isLocked = true; 
      digitalWrite(RELAY_PIN, HIGH); myServo.write(POS_LOCKED); tone(BUZZER_PIN, 2500);        
      
      if (!discordSent) { 
        xTaskNotifyGive(networkTaskHandle); // ปลุก Task Network ให้ทำงาน
        discordSent = true; 
      }
    }
  } else if (smokeValue < (THRESHOLD - HYSTERESIS)) {
    if (isSmokeDetected && !isTestMode) { 
      isSmokeDetected = false; discordSent = false; 
      digitalWrite(RELAY_PIN, LOW); noTone(BUZZER_PIN);
    }
  }
  
  updateLEDs();

  if (isSmokeDetected) {
    if (millis() - lastBuzzerTime > 200) { 
      lastBuzzerTime = millis();
      static bool bz = false; bz = !bz;
      if (bz) tone(BUZZER_PIN, 2500); else noTone(BUZZER_PIN);
    }
  } else noTone(BUZZER_PIN);

  if (digitalRead(BUTTON_PIN) == LOW) {
    if (!isButtonPressed) { isButtonPressed = true; buttonPressedTime = millis(); resetHandled = false; }
    else if (!resetHandled && (millis() - buttonPressedTime > 2000)) {
      if (sensorRead < THRESHOLD) { 
        isTestMode = false; isLocked = false; isSmokeDetected = false; discordSent = false; 
        digitalWrite(RELAY_PIN, LOW); resetHandled = true; 
      }
    }
  } else isButtonPressed = false;

  if (isLocked != lastLockedState) { 
    myServo.write(isLocked ? POS_LOCKED : POS_UNLOCKED); 
    lastLockedState = isLocked; 
  }

  if (millis() - lastDisplayTime > 200) { updateDisplay(smokeValue); lastDisplayTime = millis(); }
}