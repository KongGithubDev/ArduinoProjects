#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <Wire.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_BME280.h>
#include <ESP32Servo.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);
Adafruit_BME280 bme;
Servo maskServo;

HardwareSerial pmSerial(2);

const char* ssid = "Kong_Wifi";
const char* password = "0651166902z";

String serverURL =
"https://phylactic-unrueful-jalisa.ngrok-free.dev/air_alert";

int pm25 = 0;
String lastStatus = "";

WiFiClientSecure client;

void connectWiFi() {
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
  }
}

void readPM() {
  if (pmSerial.available() >= 32) {
    uint8_t buffer[32];
    pmSerial.readBytes(buffer, 32);
    pm25 = buffer[12] * 256 + buffer[13];
  }
}

String getStatus(int pm) {
  if (pm < 35) return "SAFE";
  else if (pm < 75) return "WARNING";
  else return "DANGER";
}

void releaseMask() {
  maskServo.write(90);
  delay(2000);
  maskServo.write(0);
}

void sendAPI(float t, float h, int pm, String status, bool mask) {

  if (WiFi.status() != WL_CONNECTED) return;

  client.setInsecure();

  HTTPClient https;
  https.begin(client, serverURL);
  https.addHeader("Content-Type", "application/json");

  String json =
    "{"
    "\"temperature\":" + String(t) + ","
    "\"humidity\":" + String(h) + ","
    "\"pm25\":" + String(pm) + ","
    "\"status\":\"" + status + "\","
    "\"mask\":" + String(mask ? "true" : "false") +
    "}";

  int httpCode = https.POST(json);
  Serial.println(httpCode);

  https.end();
}

void setup() {

  Serial.begin(115200);

  Wire.begin(21,22);

  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  display.clearDisplay();

  bme.begin(0x76);

  maskServo.attach(18);
  maskServo.write(0);

  pmSerial.begin(9600, SERIAL_8N1, 16, 17);

  connectWiFi();
}

void loop() {

  readPM();

  float temp = bme.readTemperature();
  float hum = bme.readHumidity();

  String status = getStatus(pm25);
  bool released = false;

  if (status == "DANGER" && lastStatus != "DANGER") {
    releaseMask();
    released = true;
  }

  if (status != lastStatus) {
    sendAPI(temp, hum, pm25, status, released);
    lastStatus = status;
  }

  display.clearDisplay();
  display.setCursor(0,0);
  display.setTextSize(1);

  display.println("Air Monitor");
  display.print("Temp: "); display.println(temp);
  display.print("Hum : "); display.println(hum);
  display.print("PM2.5: "); display.println(pm25);
  display.print("Status: "); display.println(status);

  display.display();

  delay(3000);
}