#include <AccelStepper.h>
#include <Servo.h>

// Use Hardware Serial1 for Bluetooth on Mega (Pins 18 RX, 19 TX)
#define Bluetooth Serial1

Servo s01, s02, s03, s04, s05, s06;

// L298 Motor Interface Type
#define MotorInterfaceType 4

// Stepper definitions for L298 (4 pins per wheel)
// *Note: I assigned unused Mega pins below. Update them to match your exact wiring!*
AccelStepper LeftBackWheel(MotorInterfaceType, 22, 24, 26, 28);
AccelStepper LeftFrontWheel(MotorInterfaceType, 23, 25, 27, 29);
AccelStepper RightBackWheel(MotorInterfaceType, 30, 32, 34, 36);
AccelStepper RightFrontWheel(MotorInterfaceType, 31, 33, 35, 37);

#define led 14
int wheelSpeed = 600; // Updated for L298 stability

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
int currentDir = 0;

void setup() {
  // Set speeds based on your new L298 stable settings
  LeftFrontWheel.setMaxSpeed(600.0);
  LeftFrontWheel.setAcceleration(300.0);
  LeftBackWheel.setMaxSpeed(600.0);
  LeftBackWheel.setAcceleration(300.0);
  RightFrontWheel.setMaxSpeed(600.0);
  RightFrontWheel.setAcceleration(300.0);
  RightBackWheel.setMaxSpeed(600.0);
  RightBackWheel.setAcceleration(300.0);

  pinMode(led, OUTPUT);

  s01.attach(5);
  s02.attach(6);
  s03.attach(7);
  s04.attach(8);
  s05.attach(9);
  s06.attach(10);

  Bluetooth.begin(115200);
  Serial.begin(115200);

  // Power-up smoothing: Avoid starting at any hardcoded specific angle.
  // We simply read the default angle assigned by attach() to initialize our tracking array,
  // then we smoothly increment/decrement towards the default home positions (tarP).
  curP[0] = s01.read();
  curP[1] = s02.read();
  curP[2] = s03.read();
  curP[3] = s04.read();
  curP[4] = s05.read();
  curP[5] = s06.read();

  // Glide smoothly from the initial attached positions to the default home (tarP)
  // We use a slower delay (~28ms per degree) so the homing sequence takes several seconds
  while (!armAtTarget()) {
    if (millis() - lastServoUpdate > 28) {
      lastServoUpdate = millis();
      smoothMove();
    }
  }
}

void loop() {
  if (Bluetooth.available() > 0 || Serial.available() > 0) {
    if (Bluetooth.available() > 0) {
      dataIn = Bluetooth.read();
    } else {
      dataIn = Serial.read();
    }

    // Commands 0-27 are movement/servo modes
    if (dataIn <= 27)
      m = dataIn;

    // Wheel Speed logic
    if (dataIn > 30 && dataIn < 100)
      wheelSpeed = dataIn * 20;

    // Arm Smoothness Speed logic
    if (dataIn > 101 && dataIn < 250)
      speedDelay = dataIn / 10;

    handleInputCommands();
  }

  // Non-blocking smooth movement execution
  if (millis() - lastServoUpdate > speedDelay) {
    lastServoUpdate = millis();
    smoothMove();
  }

  // Constant drivetrain execution (for smooth stepping)
  LeftFrontWheel.run();
  LeftBackWheel.run();
  RightFrontWheel.run();
  RightBackWheel.run();

  checkBattery();
}

void handleInputCommands() {
  // Servo Target Updates (Manual Control)
  if (m == 16)
    tarP[0] += 5;
  if (m == 17)
    tarP[0] -= 5;
  if (m == 19)
    tarP[1] += 5;
  if (m == 18)
    tarP[1] -= 5;
  if (m == 20)
    tarP[2] += 5;
  if (m == 21)
    tarP[2] -= 5;
  if (m == 23)
    tarP[3] += 5;
  if (m == 22)
    tarP[3] -= 5;
  if (m == 25)
    tarP[4] += 5;
  if (m == 24)
    tarP[4] -= 5;
  if (m == 26)
    tarP[5] += 5;
  if (m == 27)
    tarP[5] -= 5;

  for (int i = 0; i < 6; i++)
    tarP[i] = constrain(tarP[i], 0, 180);

  // Drivetrain Switch with Acceleration protection
  if (m >= 1 && m <= 10) {
    if (m != currentDir) {
      currentDir = m;
      switch (m) {
      case 2:
        moveForward();
        break;
      case 7:
        moveBackward();
        break;
      case 4:
        moveSidewaysLeft();
        break;
      case 5:
        moveSidewaysRight();
        break;
      case 1:
        moveLeftForward();
        break;
      case 3:
        moveRightForward();
        break;
      case 6:
        moveLeftBackward();
        break;
      case 8:
        moveRightBackward();
        break;
      case 9:
        rotateLeft();
        break;
      case 10:
        rotateRight();
        break;
      }
    }
  } else if (m == 0) {
    if (currentDir != 0) {
      currentDir = 0;
      stopMoving();
    }
  }

  // Save Step logic
  if (m == 12 && index < 50) {
    lbw[index] = LeftBackWheel.currentPosition();
    lfw[index] = LeftFrontWheel.currentPosition();
    rbw[index] = RightBackWheel.currentPosition();
    rfw[index] = RightFrontWheel.currentPosition();
    for (int i = 0; i < 6; i++)
      sSaved[i][index] = tarP[i];
    index++;
    m = 0; // Reset mode
  }

  if (m == 13) {
    setTargets(90, 100, 120, 95, 60, 80);
    m = 0;
  }
  if (m == 14)
    runSteps();
  if (m == 15)
    runDemo();
}

void smoothMove() {
  bool moved = false;
  if (curP[0] != tarP[0]) {
    curP[0] += (tarP[0] > curP[0]) ? 1 : -1;
    s01.write(curP[0]);
    moved = true;
  }
  if (curP[1] != tarP[1]) {
    curP[1] += (tarP[1] > curP[1]) ? 1 : -1;
    s02.write(curP[1]);
    moved = true;
  }
  if (curP[2] != tarP[2]) {
    curP[2] += (tarP[2] > curP[2]) ? 1 : -1;
    s03.write(curP[2]);
    moved = true;
  }
  if (curP[3] != tarP[3]) {
    curP[3] += (tarP[3] > curP[3]) ? 1 : -1;
    s04.write(curP[3]);
    moved = true;
  }
  if (curP[4] != tarP[4]) {
    curP[4] += (tarP[4] > curP[4]) ? 1 : -1;
    s05.write(curP[4]);
    moved = true;
  }
  if (curP[5] != tarP[5]) {
    curP[5] += (tarP[5] > curP[5]) ? 1 : -1;
    s06.write(curP[5]);
    moved = true;
  }

  if (moved) {
    Serial.print("S:");
    Serial.print(curP[0]);
    Serial.print(",");
    Serial.print(curP[1]);
    Serial.print(",");
    Serial.print(curP[2]);
    Serial.print(",");
    Serial.print(curP[3]);
    Serial.print(",");
    Serial.print(curP[4]);
    Serial.print(",");
    Serial.println(curP[5]);
  }
}

bool armAtTarget() {
  for (int i = 0; i < 6; i++)
    if (curP[i] != tarP[i])
      return false;
  return true;
}

void updateServosImmediate() {
  s01.write(curP[0]);
  s02.write(curP[1]);
  s03.write(curP[2]);
  s04.write(curP[3]);
  s05.write(curP[4]);
  s06.write(curP[5]);
}

void runSteps() {
  for (int i = 0; i < index; i++) {
    LeftFrontWheel.moveTo(lfw[i]);
    LeftBackWheel.moveTo(lbw[i]);
    RightFrontWheel.moveTo(rfw[i]);
    RightBackWheel.moveTo(rbw[i]);
    for (int s = 0; s < 6; s++)
      tarP[s] = sSaved[s][i];

    while (LeftBackWheel.distanceToGo() != 0 || !armAtTarget()) {
      LeftFrontWheel.run();
      LeftBackWheel.run();
      RightFrontWheel.run();
      RightBackWheel.run();
      if (millis() - lastServoUpdate > speedDelay) {
        lastServoUpdate = millis();
        smoothMove();
      }
      if (Bluetooth.available() > 0 && Bluetooth.read() == 13)
        return;
    }
  }
}

void runDemo() {
  // 1. Home and open gripper
  setTargets(90, 100, 120, 95, 60, 80);
  waitTarget();

  // 2. Lower to Pick
  setTargets(90, 130, 90, 95, 60, 80);
  waitTarget();

  // 3. Close Gripper
  setTargets(90, 130, 90, 95, 60, 130);
  waitTarget();

  // 4. Lift
  setTargets(90, 100, 120, 95, 60, 130);
  waitTarget();

  // 5. Rotate and Lower to Place
  setTargets(45, 130, 90, 95, 60, 130);
  waitTarget();

  // 6. Open Gripper
  setTargets(45, 130, 90, 95, 60, 80);
  waitTarget();

  // 7. Lift and return to Home
  setTargets(90, 100, 120, 95, 60, 110);
  waitTarget();

  m = 0;
}

void setTargets(int t0, int t1, int t2, int t3, int t4, int t5) {
  tarP[0] = t0;
  tarP[1] = t1;
  tarP[2] = t2;
  tarP[3] = t3;
  tarP[4] = t4;
  tarP[5] = t5;
}

void waitTarget() {
  while (!armAtTarget()) {
    if (millis() - lastServoUpdate > speedDelay) {
      lastServoUpdate = millis();
      smoothMove();
    }
  }
  delay(500);
}

void moveForward() {
  setAllMaxSpeed(wheelSpeed);
  LeftFrontWheel.move(1000000);
  LeftBackWheel.move(1000000);
  RightFrontWheel.move(1000000);
  RightBackWheel.move(1000000);
}

void moveBackward() {
  setAllMaxSpeed(wheelSpeed);
  LeftFrontWheel.move(-1000000);
  LeftBackWheel.move(-1000000);
  RightFrontWheel.move(-1000000);
  RightBackWheel.move(-1000000);
}

void moveSidewaysRight() {
  setAllMaxSpeed(wheelSpeed);
  LeftFrontWheel.move(1000000);
  LeftBackWheel.move(-1000000);
  RightFrontWheel.move(-1000000);
  RightBackWheel.move(1000000);
}

void moveSidewaysLeft() {
  setAllMaxSpeed(wheelSpeed);
  LeftFrontWheel.move(-1000000);
  LeftBackWheel.move(1000000);
  RightFrontWheel.move(1000000);
  RightBackWheel.move(-1000000);
}

void rotateLeft() {
  setAllMaxSpeed(wheelSpeed);
  LeftFrontWheel.move(-1000000);
  LeftBackWheel.move(-1000000);
  RightFrontWheel.move(1000000);
  RightBackWheel.move(1000000);
}

void rotateRight() {
  setAllMaxSpeed(wheelSpeed);
  LeftFrontWheel.move(1000000);
  LeftBackWheel.move(1000000);
  RightFrontWheel.move(-1000000);
  RightBackWheel.move(-1000000);
}

void moveRightForward() {
  setAllMaxSpeed(wheelSpeed);
  LeftFrontWheel.move(1000000);
  LeftBackWheel.stop();
  RightFrontWheel.stop();
  RightBackWheel.move(1000000);
}

void moveRightBackward() {
  setAllMaxSpeed(wheelSpeed);
  LeftFrontWheel.stop();
  LeftBackWheel.move(-1000000);
  RightFrontWheel.move(-1000000);
  RightBackWheel.stop();
}

void moveLeftForward() {
  setAllMaxSpeed(wheelSpeed);
  LeftFrontWheel.stop();
  LeftBackWheel.move(1000000);
  RightFrontWheel.move(1000000);
  RightBackWheel.stop();
}

void moveLeftBackward() {
  setAllMaxSpeed(wheelSpeed);
  LeftFrontWheel.move(-1000000);
  LeftBackWheel.stop();
  RightFrontWheel.stop();
  RightBackWheel.move(-1000000);
}

void stopMoving() {
  LeftFrontWheel.stop();
  LeftBackWheel.stop();
  RightFrontWheel.stop();
  RightBackWheel.stop();
}

void setAllMaxSpeed(int s) {
  LeftFrontWheel.setMaxSpeed(s);
  LeftBackWheel.setMaxSpeed(s);
  RightFrontWheel.setMaxSpeed(s);
  RightBackWheel.setMaxSpeed(s);
}

void checkBattery() {
  float voltage = analogRead(A0) * (5.0 / 1023.00) * 3;
  digitalWrite(led, (voltage < 11.0) ? HIGH : LOW);
}