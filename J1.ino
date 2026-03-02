#include <Wire.h>
#include <Adafruit_MLX90614.h>

#define TRIG D5
#define ECHO D6
#define LEVEL_PIN D7
#define PUMP D0

#define RELAY_ON true
#define RELAY_OFF false

#define LED_PIN LED_BUILTIN
#define LED_ON LOW
#define LED_OFF HIGH

Adafruit_MLX90614 mlx = Adafruit_MLX90614();

float handThreshold = 10.0;
unsigned long pumpDuration = 800;

unsigned long lastTriggerTime = 0;
bool pumpActive = false;

float readDistance() {
  digitalWrite(TRIG, LOW);
  delayMicroseconds(2);

  digitalWrite(TRIG, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG, LOW);

  long duration = pulseIn(ECHO, HIGH, 30000);

  if (duration == 0) return -1;

  return duration * 0.034 / 2;
}

void setup() {
  Serial.begin(115200);

  pinMode(TRIG, OUTPUT);
  pinMode(ECHO, INPUT);
  pinMode(LEVEL_PIN, INPUT);
  pinMode(LED_PIN, OUTPUT);

  delay(200);

  RelayControl(PUMP, RELAY_OFF);
  digitalWrite(LED_PIN, LED_OFF);

  Wire.begin(D2, D1);
  mlx.begin();
}

void loop() {

  float distance = readDistance();
  float bodyTemp = mlx.readObjectTempC();

  bool gelLow = digitalRead(LEVEL_PIN) == LOW;
  bool gelAvailable = !gelLow;

  Serial.print("Distance: ");
  Serial.print(distance);
  Serial.print(" cm | Temp: ");
  Serial.print(bodyTemp);
  Serial.print(" C | Gel Available: ");
  Serial.print(gelAvailable);

  if (gelAvailable) {
    digitalWrite(LED_PIN, LED_ON);
  } else {
    digitalWrite(LED_PIN, LED_OFF);
  }

  if (!gelAvailable) {
    RelayControl(PUMP, RELAY_OFF);
    pumpActive = false;
  }
  else {

    if (distance > 0 && distance < handThreshold && !pumpActive) {
      RelayControl(PUMP, RELAY_ON);
      pumpActive = true;
      lastTriggerTime = millis();
    }

    if (pumpActive && millis() - lastTriggerTime >= pumpDuration) {
      RelayControl(PUMP, RELAY_OFF);
      pumpActive = false;
    }
  }

  Serial.print(" C | PUMP Available: ");
  Serial.println(pumpActive ? "Yes" : "No");

  delay(100);
}


int RelayControl(int GPIO, bool enable) { //Fix Bugs
  Serial.printf("[RELAY] Pin: %d set to: %s\n", GPIO, enable ? "OUTPUT (Active)" : "INPUT (Inactive)");

  if(enable) {
    pinMode(GPIO, OUTPUT);
  } else {
    pinMode(GPIO, INPUT);
  }
  return 0; 
}