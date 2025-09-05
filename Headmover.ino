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

const int servoPin1 = D0;   
const int servoPin2 = D1;
const int servoPin3 = D2;

unsigned long lastChange = 0;
int lastValue = 1500;
int minValue = 988;
int maxValue = 2011;
int waitTime = 7000;

int pos1 = 90, pos2 = 90, pos3 = 90;   // Startpositionen
unsigned long lastMoveTime = 0;

void setup() {
  Serial.begin(115200);
  Serial1.begin(115200);    // iBus auf RX1 (Pin D6)
  ibus.begin(Serial1);

  servo1.attach(servoPin1);
  servo2.attach(servoPin2);
  servo3.attach(servoPin3);

  servo1.write(pos1);
  servo2.write(pos2);
  servo3.write(pos3);
  Serial.println("Start");
}

void smoothMove(Servo &servo, int &currentPos, int targetPos, int stepSize=1) {
  if (millis() - lastMoveTime > 15) { // ca. 60Hz update
    if (abs(targetPos - currentPos) > stepSize) {
      currentPos += (targetPos > currentPos) ? stepSize : -stepSize;
      servo.write(currentPos);
    } else {
      currentPos = targetPos;
      servo.write(currentPos);
    }
    lastMoveTime = millis();
  }
}

void randomMove(Servo &servo, int &currentPos) {
  static unsigned long nextChange = 0;
  static int target = 90;
  if (millis() > nextChange) {
    target = random(30, 150);
    nextChange = millis() + random(500, 2500); // 0,5–2s Pause
  }
  smoothMove(servo, currentPos, target, 2);
}

void loop() {
  ibus.loop();
  int val = ibus.readChannel(0);  // Kanal 1 = Index 0
  Serial.println(val);
  if (val < minValue || val > maxValue) val = 1500; // failsafe
  
  if (abs(val - lastValue) > 5) {
    lastChange = millis();
    lastValue = val;
  }

  if (millis() - lastChange < waitTime) {
  
    // Normale Steuerung: Servo1 folgt dem Signal
    int target1 = map(val, minValue, maxValue, 30, 150);
    smoothMove(servo1, pos1, target1, 2);
    // Servos 2+3 zufällig
    randomMove(servo2, pos2);
    randomMove(servo3, pos3);
  } else {
    // Kein Signalwechsel >waitTime → alle zufällig
    Serial.println("Random");
    randomMove(servo1, pos1);
    randomMove(servo2, pos2);
    randomMove(servo3, pos3);

  }
}
