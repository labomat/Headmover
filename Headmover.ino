/*********************************************************************
  Steuerung für einen animatronischen Kopf mittels dreier Servos und Kontrollsignal via iBus
  
  head control for animatronic figurine via 3 servos an iBus
  mit Kalibrierung, Neutralwerten und Debug-Level
  Seeduino Xiao SAMD21
  
  CC-BY SA 2025 Kai Laborenz (with a little help from Chaptgpt)
*********************************************************************/
#include <IBusBM.h>
#include <Servo.h>
#include <FlashStorage.h>

// ---------- Debug-Level ----------
// 0 = keine Ausgabe
// 1 = nur Normalmodus
// 2 = Normalmodus + Kalibrierung
#define DEBUG_LEVEL 2

// ---------- Servo-Objekte ----------
IBusBM ibus;
Servo servo1, servo2, servo3;

// GPIOs für Servos
const int servoPin1 = D0;   
const int servoPin2 = D3;
const int servoPin3 = D2;

// ---------- Flash-Speicher für Neutralwerte ----------
struct NeutralData {
  int n1;
  int n2;
  int n3;
};

FlashStorage(neutralStorage, NeutralData);
NeutralData neutral = {90, 92, 87}; // Defaultwerte

// ---------- Variablen ----------
unsigned long lastChange = 0;
int lastValue = 1500;
int minValue = 988;
int maxValue = 2011;
int waitTime = 7000;

int pos1, pos2, pos3;   // aktuelle Positionen
unsigned long lastMoveTime[3] = {0,0,0};

bool calibMode = false;

// ---------- Hilfsfunktionen ----------
void loadNeutralFromFlash() {
  NeutralData stored = neutralStorage.read();
  if (stored.n1 >= 30 && stored.n1 <= 150) neutral.n1 = stored.n1;
  if (stored.n2 >= 30 && stored.n2 <= 150) neutral.n2 = stored.n2;
  if (stored.n3 >= 30 && stored.n3 <= 150) neutral.n3 = stored.n3;

  if (DEBUG_LEVEL >= 1) {
    Serial.print("Neutralwerte geladen: ");
    Serial.print(neutral.n1); Serial.print(" | ");
    Serial.print(neutral.n2); Serial.print(" | ");
    Serial.println(neutral.n3);
  }
}

void saveNeutralToFlash() {
  neutralStorage.write(neutral);
  if (DEBUG_LEVEL >= 2) {
    Serial.print("Neutralwerte gespeichert: ");
    Serial.print(neutral.n1); Serial.print(" | ");
    Serial.print(neutral.n2); Serial.print(" | ");
    Serial.println(neutral.n3);
  }
}

// Sanfte Servobewegungen
void smoothMove(Servo &servo, int &currentPos, int targetPos, int stepSize, int idx) {
  if (millis() - lastMoveTime[idx] > 15) { 
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

// Zufallsbewegungen wenn kein Steuersignal kommt
void randomMove(Servo &servo, int &currentPos, int idx,
                int minAngle, int maxAngle,
                int neutralPos, int stepSize, int minPause, int maxPause) {
  static unsigned long nextChange[3] = {0,0,0};
  static int target[3] = {90,90,90};
  static int state[3] = {0,0,0};  // 0=Pause, 1=zufällige Bewegung, 2=zurück zur Neutralposition

  if (state[idx] == 1) {
    smoothMove(servo, currentPos, target[idx], stepSize, idx);
    if (currentPos == target[idx]) state[idx] = 2;
  } 
  else if (state[idx] == 2) {
    smoothMove(servo, currentPos, neutralPos, stepSize, idx);
    if (currentPos == neutralPos) {
      state[idx] = 0;
      nextChange[idx] = millis() + random(minPause, maxPause);
    }
  } 
  else {
    if (millis() > nextChange[idx]) {
      target[idx] = random(minAngle, maxAngle);
      state[idx] = 1;
    }
  }
}

// ---------- Setup ----------
void setup() {
  Serial.begin(115200);
  Serial1.begin(115200); 
  ibus.begin(Serial1);

  loadNeutralFromFlash();

  servo1.attach(servoPin1);
  servo2.attach(servoPin2);
  servo3.attach(servoPin3);

  pos1 = neutral.n1;
  pos2 = neutral.n2;
  pos3 = neutral.n3;

  servo1.write(pos1);
  servo2.write(pos2);
  servo3.write(pos3);

  delay(500); // kurz warten, USB & iBus initialisieren

  // Kalibrierungsmodus prüfen
  unsigned long start = millis();
  int ch10 = 0;
  while (millis() - start < 3000) { 
      ibus.loop();                    
      ch10 = ibus.readChannel(9);   
      if (ch10 != 0) break;          
      delay(1);                      
  }

  Serial.print("Kanal 10 Startwert: "); Serial.println(ch10);

  float ch10_percent = (float)(ch10 - minValue) / (maxValue - minValue) * 100.0;
  if (ch10_percent >= 90.0) {
      calibMode = true;
      if (DEBUG_LEVEL >= 2) Serial.println("⚙️ Kalibrierungsmodus aktiviert!");
  } else {
      calibMode = false;
      if (DEBUG_LEVEL >= 1) Serial.println("✅ Normalmodus mit gespeicherten Neutralwerten.");
  }

  delay(2000);
}

// ---------- Loop ----------
void loop() {
  ibus.loop();

  if (calibMode) {
    int ch10 = ibus.readChannel(9);  // Schalter für Kalibriermodus
    int ch11 = ibus.readChannel(10); // Servo 1
    int ch12 = ibus.readChannel(11); // Servo 2
    int ch13 = ibus.readChannel(12); // Servo 3

    // Kanäle in Neutralwerte mappen
    neutral.n1 = map(ch11, minValue, maxValue, 30, 150);
    neutral.n2 = map(ch12, minValue, maxValue, 30, 150);
    neutral.n3 = map(ch13, minValue, maxValue, 30, 150);

    servo1.write(neutral.n1);
    servo2.write(neutral.n2);
    servo3.write(neutral.n3);

    // Ausgabe während Kalibrierung
    if (DEBUG_LEVEL >= 2) {
      Serial.print("Kalibriere: ");
      Serial.print(neutral.n1); Serial.print(" | ");
      Serial.print(neutral.n2); Serial.print(" | ");
      Serial.println(neutral.n3);
    }

    // Beenden der Kalibrierung
    if (ch10 < 1600) {
      saveNeutralToFlash();
      calibMode = false;
      if (DEBUG_LEVEL >= 2) Serial.println("🔒 Kalibrierung abgeschlossen. Werte gespeichert!");
    }
    return; // normale Logik überspringen
  }

  // -------- Normalmodus --------
  int val = ibus.readChannel(0);  
  if (val < minValue || val > maxValue) val = 1500; 
  
  if (abs(val - lastValue) > 5) {
    lastChange = millis();
    lastValue = val;
  }

// Servo 1 dreht den Kopf, Servo 2 und 3 machen kleine Zufallsbewegungen 
  if (millis() - lastChange < waitTime) {
    int target1 = map(val, minValue, maxValue, 30, 150);
    smoothMove(servo1, pos1, target1, 2, 0);

    randomMove(servo2, pos2, 1, 50, 105, neutral.n2, 2, 2000, 6000);
    randomMove(servo3, pos3, 2, 60, 100, neutral.n3, 1, 2000, 6000);

    // Ausgabe Normalmodus
    if (DEBUG_LEVEL >= 1) {
      Serial.print("Sig: "); Serial.print(val);
      Serial.print(" | "); Serial.print(target1);
      Serial.print(" Servos: ");
      Serial.print(pos1); Serial.print(" | ");
      Serial.print(pos2); Serial.print(" | ");
      Serial.println(pos3);
    }

// Wenn kein Steuersignal anliegt, bewegt sich der Kopf in unregelmäßigen Abständen
  } else {
    randomMove(servo1, pos1, 0, 30, 150, neutral.n1, 2, 2000, 6000);
    randomMove(servo2, pos2, 1, 40, 100, neutral.n2, 2, 2000, 6000);
    randomMove(servo3, pos3, 2, 50, 105, neutral.n3, 1, 2000, 6000);

    if (DEBUG_LEVEL >= 1) {
      Serial.print("R Sig: "); Serial.print(val);
      Serial.print(" Servos: ");
      Serial.print(pos1); Serial.print(" | ");
      Serial.print(pos2); Serial.print(" | ");
      Serial.println(pos3); 
    }
  }
}
