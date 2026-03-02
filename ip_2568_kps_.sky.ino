#define BLYNK_TEMPLATE_ID "TMPL69I3MwlF8"
#define BLYNK_TEMPLATE_NAME "Smart Pet Cage"
#define BLYNK_AUTH_TOKEN "hFsrbZi1BmIhKZmPUAux8W9ixqh-taib"

#include <WiFi.h>
#include <BlynkSimpleEsp32.h>
#include <ESP32Servo.h>

// Wi-Fi credentials
char ssid[] = "MyInternet";
char pass[] = "000000000";

// Pins
#define LOCK_MOTOR_PIN 14         // Servo motor for cage lock
#define WATER_RELAY_PIN 4         // Relay for water pump control
#define FOOD_MOTOR_PIN 15         // Servo motor for food dispenser
const int trigPin = 12;           // Trigger pin for HC-SR04 (food sensor)
const int echoPin = 13;           // Echo pin for HC-SR04 (food sensor)
const int waterTrigPin = 16;      // Trigger pin for HC-SR04 (water sensor)
const int waterEchoPin = 17;      // Echo pin for HC-SR04 (water sensor)

// Servo objects
Servo lockMotor;
Servo foodmotor;

long duration;
int distance;
long waterDuration;
int waterDistance;

void setup() {
  Serial.begin(9600);
  
  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);
  pinMode(waterTrigPin, OUTPUT);
  pinMode(waterEchoPin, INPUT);
  pinMode(WATER_RELAY_PIN, OUTPUT);  

  Blynk.begin(BLYNK_AUTH_TOKEN, ssid, pass);

  lockMotor.attach(LOCK_MOTOR_PIN);
  foodmotor.attach(FOOD_MOTOR_PIN);

  digitalWrite(WATER_RELAY_PIN, LOW);  // Initial state OFF
  lockMotor.write(0);                  // Locked position
  foodmotor.write(0);
}

// Water pump control
BLYNK_WRITE(V1) {
  int waterState = param.asInt();
  Serial.print("Water state: ");
  Serial.println(waterState);
  digitalWrite(WATER_RELAY_PIN, waterState == 1 ? HIGH : LOW);
}

// Cage lock/unlock control
BLYNK_WRITE(V3) {
  int lockState = param.asInt();
  if (lockState == 1) {
    lockMotor.write(90);
    delay(1000);
  } else {
    lockMotor.write(0);
  }
}

// Food dispensing control
BLYNK_WRITE(V2) {
  int foodState = param.asInt();
  if (foodState == 1) {
    foodmotor.write(90);
    delay(2000);
    foodmotor.write(0);
  }
}

void loop() {
  // Read food level
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);
  duration = pulseIn(echoPin, HIGH);
  distance = duration * 0.0344 / 2;

  delay(50); // Prevent sensor overlap

  // Read water level
  digitalWrite(waterTrigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(waterTrigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(waterTrigPin, LOW);
  waterDuration = pulseIn(waterEchoPin, HIGH);
  waterDistance = waterDuration * 0.0344 / 2;

  Serial.print("Water Level Distance: ");
  Serial.println(waterDistance);

  // Auto water refill
  if (waterDistance > 10) {
    digitalWrite(WATER_RELAY_PIN, HIGH);
    delay(5000);
  } else {
    digitalWrite(WATER_RELAY_PIN, LOW);
  }

  // Auto food dispense
  if (distance < 10) {
    foodmotor.write(90);
    delay(500);
  } else {
    foodmotor.write(0);
  }

  delay(500);
  Blynk.run();
}
