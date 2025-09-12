/*********************************************************************
  Headmover

  Steuerung für einen animatronischen Kopf mittels dreier Servos und Kontrollsignal via iBus
  
  head control for animatronic figurine via 3 servos an iBus
  
  CC-BY SA 2025 Kai Laborenz
*********************************************************************/
#include <IBusBM.h>
#include <Servo.h>

IBusBM ibus;
Servo servo1, servo2, servo3;

// ⚠️ bitte anpassen: funktionierende GPIOs für dein Board!
const int servoPin1 = D0;   
const int servoPin2 = D3;
const int servoPin3 = D2;

unsigned long lastChange = 0;
int lastValue = 1500;
int minValue = 988;
int maxValue = 2011;
int waitTime = 7000;

int pos1 = 90, pos2 = 90, pos3 = 90;   // Startpositionen
unsigned long lastMoveTime[3] = {0,0,0};  // getrennte Timer für jede Servoachse

void setup() {
  Serial.begin(115200);
  Serial1.begin(115200);    // iBus auf RX1 (Pin D6)
  ibus.begin(Serial1);

  servo1.attach(servoPin1);
  servo2.attach(servoPin2);
  servo3.attach(servoPin3);

  Serial.println("Startpositionen");
  servo1.write(pos1);
  servo2.write(pos2);
  servo3.write(pos3);
  delay(2000);
  Serial.println("Los gehts");
}

void smoothMove(Servo &servo, int &currentPos, int targetPos, int stepSize, int idx) {
  if (millis() - lastMoveTime[idx] > 15) { // ca. 60Hz update
    if (abs(targetPos - currentPos) > stepSize) {
      currentPos += (targetPos > currentPos) ? stepSize : -stepSize;
      servo.write(currentPos);
    } else {
      currentPos = targetPos;
      servo.write(currentPos);
    }
    lastMoveTime[idx] = millis();
  }
}

void randomMove(Servo &servo, int &currentPos, int idx,
                int minAngle, int maxAngle,
                int stepSize, int minPause, int maxPause) {
  static unsigned long nextChange[3] = {0,0,0};
  static int target[3] = {90,90,90};
  static int state[3] = {0,0,0};  // 0=Pause, 1=zufällige Bewegung, 2=zurück zur Mitte

  if (state[idx] == 1) {
    // Bewegung zum Zufallsziel
    smoothMove(servo, currentPos, target[idx], stepSize, idx);
    if (currentPos == target[idx]) {
      state[idx] = 2;  // Ziel erreicht → zurück zur Mitte
    }
  } 
  else if (state[idx] == 2) {
    // Bewegung zur Neutralposition
    smoothMove(servo, currentPos, 90, stepSize, idx);
    if (currentPos == 90) {
      state[idx] = 0;  // Mitte erreicht → Pause
      nextChange[idx] = millis() + random(minPause, maxPause);
    }
  } 
  else { // state == 0 → Pause
    if (millis() > nextChange[idx]) {
      target[idx] = random(minAngle, maxAngle);
      state[idx] = 1;  // neue Bewegung starten
    }
  }
}


void loop() {
  ibus.loop();
  int val = ibus.readChannel(0);  // Kanal 1 = Index 0
  if (val < minValue || val > maxValue) val = 1500; // failsafe
  
  if (abs(val - lastValue) > 5) {
    lastChange = millis();
    lastValue = val;
  }

  if (millis() - lastChange < waitTime) {
    // Servo 1 folgt dem Signal (30–150°)
    int target1 = map(val, minValue, maxValue, 30, 150);
    smoothMove(servo1, pos1, target1, 2, 0);

    // Servo 2 & 3 mit Pausenlogik
    randomMove(servo2, pos2, 1, 50, 105, 2, 2000, 6000);
    randomMove(servo3, pos3, 2, 40, 100, 1, 2000, 6000);

    Serial.print("Sig: ");
    Serial.print(val);
    Serial.print(" | ");
    Serial.print(target1);
    Serial.print(" Servos: ");
    Serial.print(pos1);
    Serial.print(" | ");
    Serial.print(pos2);
    Serial.print(" | ");
    Serial.println(pos3);

  } else {
    // Kein Signalwechsel > waitTime → alle zufällig
    randomMove(servo1, pos1, 0, 30, 150, 2, 2000, 6000);
    randomMove(servo2, pos2, 1, 40, 100, 2, 2000, 6000);
    randomMove(servo3, pos3, 2, 50, 105, 1, 2000, 6000);

    Serial.print("Sig: ");
    Serial.print(val);
    Serial.print(" Servos:");
    Serial.print(pos1);
    Serial.print(" | ");
    Serial.print(pos2);
    Serial.print(" | ");
    Serial.print(pos3);
    Serial.println(" R");
  }
}
