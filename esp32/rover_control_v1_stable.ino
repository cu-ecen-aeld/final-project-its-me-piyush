// ===============================
// Rover Control v1.0 (STABLE)
// - Dual L298N working
// - WASD manual control
// - Auto test on boot
// ===============================

#define L_IN1 16
#define L_IN2 17
#define L_IN3 18
#define L_IN4 19
#define L_ENA 21
#define L_ENB 22

#define R_IN1 23
#define R_IN2 25
#define R_IN3 26
#define R_IN4 27
#define R_ENA 32
#define R_ENB 33

const int pwmFreq = 1000;
const int pwmResolution = 8;

int speedVal = 180;
bool testMode = true;

// ===== Movement =====

void stopRover() {
  digitalWrite(L_IN1, LOW);
  digitalWrite(L_IN2, LOW);
  digitalWrite(L_IN3, LOW);
  digitalWrite(L_IN4, LOW);

  digitalWrite(R_IN1, LOW);
  digitalWrite(R_IN2, LOW);
  digitalWrite(R_IN3, LOW);
  digitalWrite(R_IN4, LOW);
}

void moveForward() {
  digitalWrite(L_IN1, HIGH);
  digitalWrite(L_IN2, LOW);
  digitalWrite(L_IN3, HIGH);
  digitalWrite(L_IN4, LOW);

  digitalWrite(R_IN1, HIGH);
  digitalWrite(R_IN2, LOW);
  digitalWrite(R_IN3, HIGH);
  digitalWrite(R_IN4, LOW);
}

void moveBackward() {
  digitalWrite(L_IN1, LOW);
  digitalWrite(L_IN2, HIGH);
  digitalWrite(L_IN3, LOW);
  digitalWrite(L_IN4, HIGH);

  digitalWrite(R_IN1, LOW);
  digitalWrite(R_IN2, HIGH);
  digitalWrite(R_IN3, LOW);
  digitalWrite(R_IN4, HIGH);
}

void turnLeft() {
  digitalWrite(L_IN1, LOW);
  digitalWrite(L_IN2, HIGH);
  digitalWrite(L_IN3, LOW);
  digitalWrite(L_IN4, HIGH);

  digitalWrite(R_IN1, HIGH);
  digitalWrite(R_IN2, LOW);
  digitalWrite(R_IN3, HIGH);
  digitalWrite(R_IN4, LOW);
}

void turnRight() {
  digitalWrite(L_IN1, HIGH);
  digitalWrite(L_IN2, LOW);
  digitalWrite(L_IN3, HIGH);
  digitalWrite(L_IN4, LOW);

  digitalWrite(R_IN1, LOW);
  digitalWrite(R_IN2, HIGH);
  digitalWrite(R_IN3, LOW);
  digitalWrite(R_IN4, HIGH);
}

// ===== PWM =====

void setSpeed(int val) {
  speedVal = val;

  ledcWrite(L_ENA, speedVal);
  ledcWrite(L_ENB, speedVal);
  ledcWrite(R_ENA, speedVal);
  ledcWrite(R_ENB, speedVal);

  Serial.print("Speed set to: ");
  Serial.println(speedVal);
}

void setupPWM() {
  if (!ledcAttach(L_ENA, pwmFreq, pwmResolution)) {
    Serial.println("Failed to attach L_ENA");
    while (true) {}
  }

  if (!ledcAttach(L_ENB, pwmFreq, pwmResolution)) {
    Serial.println("Failed to attach L_ENB");
    while (true) {}
  }

  if (!ledcAttach(R_ENA, pwmFreq, pwmResolution)) {
    Serial.println("Failed to attach R_ENA");
    while (true) {}
  }

  if (!ledcAttach(R_ENB, pwmFreq, pwmResolution)) {
    Serial.println("Failed to attach R_ENB");
    while (true) {}
  }

  setSpeed(speedVal);
}

// ===== Pins =====

void setupPins() {
  pinMode(L_IN1, OUTPUT);
  pinMode(L_IN2, OUTPUT);
  pinMode(L_IN3, OUTPUT);
  pinMode(L_IN4, OUTPUT);

  pinMode(R_IN1, OUTPUT);
  pinMode(R_IN2, OUTPUT);
  pinMode(R_IN3, OUTPUT);
  pinMode(R_IN4, OUTPUT);
}

// ===== Help =====

void printHelp() {
  Serial.println();
  Serial.println("Commands:");
  Serial.println("w = forward");
  Serial.println("s = backward");
  Serial.println("a = turn left");
  Serial.println("d = turn right");
  Serial.println("space = stop");
  Serial.println("1 = speed 120");
  Serial.println("2 = speed 180");
  Serial.println("3 = speed 220");
  Serial.println("h = help");
  Serial.println();
}

// ===== Auto Test =====

void runTestStep(const char *label, void (*action)(), int durationMs) {
  Serial.println(label);
  action();
  delay(durationMs);
}

void runAutoTestCycle() {
  setSpeed(120);
  runTestStep("Test Forward @120", moveForward, 2000);
  runTestStep("Test Backward @120", moveBackward, 2000);
  runTestStep("Test Left @120", turnLeft, 1500);
  runTestStep("Test Right @120", turnRight, 1500);
  runTestStep("Test Stop", stopRover, 800);

  setSpeed(180);
  runTestStep("Test Forward @180", moveForward, 2000);
  runTestStep("Test Backward @180", moveBackward, 2000);
  runTestStep("Test Left @180", turnLeft, 1500);
  runTestStep("Test Right @180", turnRight, 1500);
  runTestStep("Test Stop", stopRover, 800);

  setSpeed(220);
  runTestStep("Test Forward @220", moveForward, 2000);
  runTestStep("Test Backward @220", moveBackward, 2000);
  runTestStep("Test Left @220", turnLeft, 1500);
  runTestStep("Test Right @220", turnRight, 1500);
  runTestStep("Test Stop", stopRover, 1000);
}

void setup() {
  Serial.begin(115200);
  setupPins();
  setupPWM();
  stopRover();

  Serial.println("Rover Ready");
  Serial.println("Auto test starts on boot.");
  Serial.println("Press any key to exit auto test and enter manual mode.");
}

void loop() {
  if (testMode) {
    if (Serial.available()) {
      while (Serial.available()) {
        Serial.read();
      }
      testMode = false;
      stopRover();
      Serial.println("Exited auto test. Manual mode enabled.");
      printHelp();
      return;
    }

    runAutoTestCycle();
    return;
  }

  if (Serial.available()) {
    char cmd = Serial.read();

    if (cmd == '\n' || cmd == '\r') {
      return;
    }

    switch (cmd) {
      case 'w':
        Serial.println("Forward");
        moveForward();
        break;

      case 's':
        Serial.println("Backward");
        moveBackward();
        break;

      case 'a':
        Serial.println("Left");
        turnLeft();
        break;

      case 'd':
        Serial.println("Right");
        turnRight();
        break;

      case ' ':
        Serial.println("Stop");
        stopRover();
        break;

      case '1':
        setSpeed(120);
        break;

      case '2':
        setSpeed(180);
        break;

      case '3':
        setSpeed(220);
        break;

      case 'h':
        printHelp();
        break;

      default:
        Serial.print("Unknown: ");
        Serial.println(cmd);
        break;
    }
  }
}
