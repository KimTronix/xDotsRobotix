#include <AccelStepper.h>
#include <Servo.h>

// Use Hardware Serial1 for Bluetooth on Mega (Pins 18 RX, 19 TX)
#define Bluetooth Serial1

Servo s01, s02, s03, s04, s05, s06;

// Stepper definitions (Pins matching your Mega setup)
AccelStepper LeftBackWheel(1, 42, 43);
AccelStepper LeftFrontWheel(1, 40, 41);
AccelStepper RightBackWheel(1, 44, 45);
AccelStepper RightFrontWheel(1, 46, 47);

#define led 14
int wheelSpeed = 1500;

// Current and Target positions for smooth interpolation
int curP[6] = {90, 100, 120, 95, 60, 110}; 
int tarP[6] = {90, 100, 120, 95, 60, 110};

// Storage arrays for "SAVE/RUN"
int lbw[50], lfw[50], rbw[50], rfw[50];
int sSaved[6][50];

int speedDelay = 20; // Lower = faster arm movement
unsigned long lastServoUpdate = 0;
int index = 0;
int dataIn;
int m = 0;

void setup() {
  LeftFrontWheel.setMaxSpeed(3000);
  LeftBackWheel.setMaxSpeed(3000);
  RightFrontWheel.setMaxSpeed(3000);
  RightBackWheel.setMaxSpeed(3000);
  
  pinMode(led, OUTPUT);
  
  s01.attach(5); s02.attach(6); s03.attach(7); 
  s04.attach(8); s05.attach(9); s06.attach(10);

  Bluetooth.begin(38400);
  Serial.begin(38400);

  // Power-up smoothing: Start from neutral and glide to initial targets
  for(int i=0; i<6; i++) curP[i] = 90; 
  updateServosImmediate();
  
  // Glide smoothly to startup positions defined in tarP
  while(!armAtTarget()) {
    if (millis() - lastServoUpdate > 30) {
      lastServoUpdate = millis();
      smoothMove();
    }
  }
}

void loop() {
  if (Bluetooth.available() > 0) {
    dataIn = Bluetooth.read();
    
    // Commands 0-27 are movement/servo modes
    if (dataIn <= 27) m = dataIn;

    // Wheel Speed logic
    if (dataIn > 30 && dataIn < 100) wheelSpeed = dataIn * 20;

    // Arm Smoothness Speed logic
    if (dataIn > 101 && dataIn < 250) speedDelay = dataIn / 10;
    
    handleInputCommands();
  }

  // Non-blocking smooth movement execution
  if (millis() - lastServoUpdate > speedDelay) {
    lastServoUpdate = millis();
    smoothMove();
  }

  // Constant drivetrain execution (for smooth stepping)
  LeftFrontWheel.runSpeed();
  LeftBackWheel.runSpeed();
  RightFrontWheel.runSpeed();
  RightBackWheel.runSpeed();

  checkBattery();
}

void handleInputCommands() {
  // Servo Target Updates (Manual Control)
  if (m == 16) tarP[0]++; if (m == 17) tarP[0]--;
  if (m == 19) tarP[1]++; if (m == 18) tarP[1]--;
  if (m == 20) tarP[2]++; if (m == 21) tarP[2]--;
  if (m == 23) tarP[3]++; if (m == 22) tarP[3]--;
  if (m == 25) tarP[4]++; if (m == 24) tarP[4]--;
  if (m == 26) tarP[5]++; if (m == 27) tarP[5]--;

  for(int i=0; i<6; i++) tarP[i] = constrain(tarP[i], 0, 180);

  // Drivetrain Switch
  switch(m) {
    case 2: moveForward(); break;
    case 7: moveBackward(); break;
    case 4: moveSidewaysLeft(); break;
    case 5: moveSidewaysRight(); break;
    case 1: moveLeftForward(); break;
    case 3: moveRightForward(); break;
    case 6: moveLeftBackward(); break;
    case 8: moveRightBackward(); break;
    case 9: rotateLeft(); break;
    case 10: rotateRight(); break;
    case 0: stopMoving(); break;
  }

  // Save Step logic
  if (m == 12 && index < 50) {
    lbw[index] = LeftBackWheel.currentPosition();
    lfw[index] = LeftFrontWheel.currentPosition();
    rbw[index] = RightBackWheel.currentPosition();
    rfw[index] = RightFrontWheel.currentPosition();
    for(int i=0; i<6; i++) sSaved[i][index] = tarP[i];
    index++;
    m = 0; // Reset mode
  }

  if (m == 14) runSteps();
}

void smoothMove() {
  if (curP[0] != tarP[0]) { curP[0] += (tarP[0] > curP[0]) ? 1 : -1; s01.write(curP[0]); }
  if (curP[1] != tarP[1]) { curP[1] += (tarP[1] > curP[1]) ? 1 : -1; s02.write(curP[1]); }
  if (curP[2] != tarP[2]) { curP[2] += (tarP[2] > curP[2]) ? 1 : -1; s03.write(curP[2]); }
  if (curP[3] != tarP[3]) { curP[3] += (tarP[3] > curP[3]) ? 1 : -1; s04.write(curP[3]); }
  if (curP[4] != tarP[4]) { curP[4] += (tarP[4] > curP[4]) ? 1 : -1; s05.write(curP[4]); }
  if (curP[5] != tarP[5]) { curP[5] += (tarP[5] > curP[5]) ? 1 : -1; s06.write(curP[5]); }
}

bool armAtTarget() {
  for(int i=0; i<6; i++) if(curP[i] != tarP[i]) return false;
  return true;
}

void updateServosImmediate() {
  s01.write(curP[0]); s02.write(curP[1]); s03.write(curP[2]);
  s04.write(curP[3]); s05.write(curP[4]); s06.write(curP[5]);
}

void runSteps() {
  for (int i = 0; i < index; i++) {
    LeftFrontWheel.moveTo(lfw[i]);
    LeftBackWheel.moveTo(lbw[i]);
    RightFrontWheel.moveTo(rfw[i]);
    RightBackWheel.moveTo(rbw[i]);
    for(int s=0; s<6; s++) tarP[s] = sSaved[s][i];

    while(LeftBackWheel.distanceToGo() != 0 || !armAtTarget()) {
      LeftFrontWheel.run(); LeftBackWheel.run();
      RightFrontWheel.run(); RightBackWheel.run();
      if (millis() - lastServoUpdate > speedDelay) {
        lastServoUpdate = millis();
        smoothMove();
      }
      if (Bluetooth.available() > 0 && Bluetooth.read() == 13) return; 
    }
  }
}

void moveForward() {
  LeftFrontWheel.setSpeed(wheelSpeed); LeftBackWheel.setSpeed(wheelSpeed);
  RightFrontWheel.setSpeed(wheelSpeed); RightBackWheel.setSpeed(wheelSpeed);
}

void moveBackward() {
  LeftFrontWheel.setSpeed(-wheelSpeed); LeftBackWheel.setSpeed(-wheelSpeed);
  RightFrontWheel.setSpeed(-wheelSpeed); RightBackWheel.setSpeed(-wheelSpeed);
}

void moveSidewaysRight() {
  LeftFrontWheel.setSpeed(wheelSpeed); LeftBackWheel.setSpeed(-wheelSpeed);
  RightFrontWheel.setSpeed(-wheelSpeed); RightBackWheel.setSpeed(wheelSpeed);
}

void moveSidewaysLeft() {
  LeftFrontWheel.setSpeed(-wheelSpeed); LeftBackWheel.setSpeed(wheelSpeed);
  RightFrontWheel.setSpeed(wheelSpeed); RightBackWheel.setSpeed(-wheelSpeed);
}

void rotateLeft() {
  LeftFrontWheel.setSpeed(-wheelSpeed); LeftBackWheel.setSpeed(-wheelSpeed);
  RightFrontWheel.setSpeed(wheelSpeed); RightBackWheel.setSpeed(wheelSpeed);
}

void rotateRight() {
  LeftFrontWheel.setSpeed(wheelSpeed); LeftBackWheel.setSpeed(wheelSpeed);
  RightFrontWheel.setSpeed(-wheelSpeed); RightBackWheel.setSpeed(-wheelSpeed);
}

void moveRightForward() {
  LeftFrontWheel.setSpeed(wheelSpeed); LeftBackWheel.setSpeed(0);
  RightFrontWheel.setSpeed(0); RightBackWheel.setSpeed(wheelSpeed);
}

void moveRightBackward() {
  LeftFrontWheel.setSpeed(0); LeftBackWheel.setSpeed(-wheelSpeed);
  RightFrontWheel.setSpeed(-wheelSpeed); RightBackWheel.setSpeed(0);
}

void moveLeftForward() {
  LeftFrontWheel.setSpeed(0); LeftBackWheel.setSpeed(wheelSpeed);
  RightFrontWheel.setSpeed(wheelSpeed); RightBackWheel.setSpeed(0);
}

void moveLeftBackward() {
  LeftFrontWheel.setSpeed(-wheelSpeed); LeftBackWheel.setSpeed(0);
  RightFrontWheel.setSpeed(0); RightBackWheel.setSpeed(-wheelSpeed);
}

void stopMoving() {
  LeftFrontWheel.setSpeed(0); LeftBackWheel.setSpeed(0);
  RightFrontWheel.setSpeed(0); RightBackWheel.setSpeed(0);
}

void checkBattery() {
  float voltage = analogRead(A0) * (5.0 / 1023.00) * 3;
  digitalWrite(led, (voltage < 11.0) ? HIGH : LOW);
}