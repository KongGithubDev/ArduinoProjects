#include <ESP32Servo.h>

#define SOIL_PIN 34
#define LDR_PIN 12
#define MOTION_PIN 15
#define RELAY_PIN 17
#define LED_PIN 2
#define SERVO_PIN 5

Servo doorServo;

int soilThreshold = 1500;
int soilHysteresis = 150;
int lightThreshold = 1000;
unsigned long pumpDuration = 3000;
// ===========================

unsigned long lastPumpTime = 0;
bool pumping = false;

unsigned long doorTimer = 0;
bool doorOpen = false;

int readSoil() {
  long total = 0;
  for(int i = 0; i < 10; i++) {
    total += analogRead(SOIL_PIN);
    delay(5);
  }
  return total / 10;
}

int readLight() {
  long total = 0;
  for(int i = 0; i < 10; i++) {
    total += analogRead(LDR_PIN);
    delay(5);
  }
  return total / 10;
}

void setup() {
  Serial.begin(115200);

  analogSetAttenuation(ADC_11db);

  pinMode(MOTION_PIN, INPUT);
  pinMode(RELAY_PIN, OUTPUT);
  pinMode(LED_PIN, OUTPUT);

  digitalWrite(RELAY_PIN, HIGH);
  digitalWrite(LED_PIN, LOW);

  doorServo.attach(SERVO_PIN);
  doorServo.write(0);
}

void loop() {

  unsigned long currentMillis = millis();

  int soilValue = readSoil();
  int lightValue = readLight();
  int motion = digitalRead(MOTION_PIN);

  Serial.print("Soil: ");
  Serial.print(soilValue);
  Serial.print(" | Light: ");
  Serial.print(lightValue);
  Serial.print(" | Motion: ");
  Serial.println(motion);

  if (!pumping && soilValue > soilThreshold) {
    digitalWrite(RELAY_PIN, LOW);
    pumping = true;
    Serial.println("เริ่มรดน้ำ");
  }

  if (pumping && soilValue < soilThreshold - soilHysteresis) {
    digitalWrite(RELAY_PIN, HIGH);
    pumping = false;
    Serial.println("ดินชื้นพอ หยุดรดน้ำ");
  }

  if (lightValue < lightThreshold) {
    digitalWrite(LED_PIN, HIGH);
  } else {
    digitalWrite(LED_PIN, LOW);
  }

  if (motion == LOW && !doorOpen) {
    doorServo.write(90);
    doorOpen = true;
    doorTimer = currentMillis;
    Serial.println("เปิดประตู");
  }

  if (doorOpen && currentMillis - doorTimer >= 3000) {
    doorServo.write(0);
    doorOpen = false;
    Serial.println("ปิดประตู");
  }

  delay(200);
}