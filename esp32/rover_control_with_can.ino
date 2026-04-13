#include <SPI.h>
#include <mcp_can.h>

// ============================================================
// USER WIRING
// ============================================================

// MCP2515 -> ESP32
static const int CAN_CS_PIN   = 5;
static const int CAN_SO_PIN   = 12;   // MISO
static const int CAN_SI_PIN   = 13;   // MOSI
static const int CAN_SCK_PIN  = 14;
static const int CAN_INT_PIN  = 4;

// Left L298N -> ESP32
static const int L_ENA = 21;
static const int L_IN1 = 16;
static const int L_IN2 = 17;
static const int L_IN3 = 18;
static const int L_IN4 = 19;
static const int L_ENB = 22;

// Right L298N -> ESP32
static const int R_ENA = 32;
static const int R_IN1 = 23;
static const int R_IN2 = 25;
static const int R_IN3 = 26;
static const int R_IN4 = 27;
static const int R_ENB = 33;

// ============================================================
// CAN CONFIG
// ============================================================

static const uint32_t CAN_RX_ID_DRIVE      = 0x1F0;
static const uint32_t CAN_RX_ID_PI_HB      = 0x110;

static const uint32_t CAN_TX_ID_ESP_HB     = 0x120;
static const uint32_t CAN_TX_ID_ESP_STATUS = 0x121;
static const uint32_t CAN_TX_ID_ESP_TLM    = 0x122;

static const byte CAN_SPEED = CAN_500KBPS;
// Change to MCP_16MHZ if your MCP2515 board crystal is 16MHz
static const byte CAN_CLOCK = MCP_8MHZ;

MCP_CAN CAN0(CAN_CS_PIN);

// ============================================================
// MOTOR PWM
// ============================================================

static const int PWM_FREQ = 20000;
static const int PWM_RES  = 8;
static const int PWM_MAX  = 255;

static const int CH_L_ENA = 0;
static const int CH_L_ENB = 1;
static const int CH_R_ENA = 2;
static const int CH_R_ENB = 3;

// ============================================================
// COMMANDS
// ============================================================

static const uint8_t CMD_STOP  = 0x00;
static const uint8_t CMD_DRIVE = 0x01;
static const uint8_t CMD_ESTOP = 0x02;

static const uint8_t MODE_OPEN_LOOP = 0x01;

static const uint8_t FLAG_ENABLE = 0x01;
static const uint8_t FLAG_ESTOP  = 0x02;

// ============================================================
// TIMING
// ============================================================

static const unsigned long HB_TX_PERIOD_MS     = 1000;
static const unsigned long STATUS_TX_PERIOD_MS = 1000;
static const unsigned long TLM_TX_PERIOD_MS    = 500;
static const unsigned long PI_TIMEOUT_MS       = 1500;

// ============================================================
// STATE
// ============================================================

bool canReady = false;
bool manualMode = true;
bool estopActive = false;
bool piAlive = false;

int manualSpeed = 180;

int currentLeftCmd  = 0;   // -255 to 255
int currentRightCmd = 0;   // -255 to 255

uint8_t lastCmd   = CMD_STOP;
uint8_t lastMode  = MODE_OPEN_LOOP;
int8_t  lastLeftPercent  = 0;
int8_t  lastRightPercent = 0;
uint8_t lastSeq   = 0;
uint8_t lastFlags = 0;
uint8_t lastRamp  = 0;
uint8_t lastDebug = 0;

unsigned long lastPiSeenMs    = 0;
unsigned long lastHbTxMs      = 0;
unsigned long lastStatusTxMs  = 0;
unsigned long lastTlmTxMs     = 0;

// ============================================================
// HELPERS
// ============================================================

int clampValue(int v, int lo, int hi) {
  if (v < lo) return lo;
  if (v > hi) return hi;
  return v;
}

int percentToPwm(int8_t percent) {
  int p = clampValue((int)percent, -100, 100);
  return (p * PWM_MAX) / 100;
}

void printFrame(const char *prefix, unsigned long id, byte len, const byte *buf) {
  Serial.printf("%s ID=0x%03lX LEN=%u DATA=", prefix, id, len);
  for (byte i = 0; i < len; i++) {
    Serial.printf("%02X", buf[i]);
    if (i < len - 1) Serial.print(" ");
  }
  Serial.println();
}

void motorOne(int in1, int in2, int pwmChannel, int cmd) {
  int pwm = abs(cmd);
  pwm = clampValue(pwm, 0, PWM_MAX);

  if (cmd > 0) {
    digitalWrite(in1, HIGH);
    digitalWrite(in2, LOW);
  } else if (cmd < 0) {
    digitalWrite(in1, LOW);
    digitalWrite(in2, HIGH);
  } else {
    digitalWrite(in1, LOW);
    digitalWrite(in2, LOW);
  }

  ledcWrite(pwmChannel, pwm);
}

void applyMotors(int leftCmd, int rightCmd) {
  leftCmd = clampValue(leftCmd, -PWM_MAX, PWM_MAX);
  rightCmd = clampValue(rightCmd, -PWM_MAX, PWM_MAX);

  currentLeftCmd = leftCmd;
  currentRightCmd = rightCmd;

  // Left driver has 2 motors: A and B
  motorOne(L_IN1, L_IN2, CH_L_ENA, leftCmd);
  motorOne(L_IN3, L_IN4, CH_L_ENB, leftCmd);

  // Right driver has 2 motors: A and B
  motorOne(R_IN1, R_IN2, CH_R_ENA, rightCmd);
  motorOne(R_IN3, R_IN4, CH_R_ENB, rightCmd);
}

void stopAllMotors() {
  applyMotors(0, 0);
}

void setDrive(int leftCmd, int rightCmd, const char *reason) {
  if (estopActive) {
    Serial.printf("[MOTOR] Ignored command because ESTOP is active: %s\n", reason);
    stopAllMotors();
    return;
  }

  applyMotors(leftCmd, rightCmd);
  Serial.printf("[MOTOR] %s -> left=%d right=%d\n", reason, currentLeftCmd, currentRightCmd);
}

void activateEstop(const char *reason) {
  estopActive = true;
  stopAllMotors();
  Serial.printf("[SAFETY] ESTOP ACTIVE: %s\n", reason);
}

void clearEstop(const char *reason) {
  estopActive = false;
  Serial.printf("[SAFETY] ESTOP CLEARED: %s\n", reason);
}

void printHelp() {
  Serial.println();
  Serial.println("=========== SERIAL COMMANDS ===========");
  Serial.println("h : help");
  Serial.println("m : toggle manual/can mode");
  Serial.println("w : forward");
  Serial.println("s : reverse");
  Serial.println("a : left turn");
  Serial.println("d : right turn");
  Serial.println("x : stop");
  Serial.println("t : self test");
  Serial.println("b : send heartbeat/status/telemetry now");
  Serial.println("p : print state");
  Serial.println("0..9 : set manual speed");
  Serial.println("=======================================");
  Serial.println();
}

void printState() {
  Serial.println();
  Serial.println("============= STATE =============");
  Serial.printf("manualMode       : %s\n", manualMode ? "true" : "false");
  Serial.printf("estopActive      : %s\n", estopActive ? "true" : "false");
  Serial.printf("piAlive          : %s\n", piAlive ? "true" : "false");
  Serial.printf("manualSpeed      : %d\n", manualSpeed);
  Serial.printf("currentLeftCmd   : %d\n", currentLeftCmd);
  Serial.printf("currentRightCmd  : %d\n", currentRightCmd);
  Serial.printf("lastCmd          : 0x%02X\n", lastCmd);
  Serial.printf("lastMode         : 0x%02X\n", lastMode);
  Serial.printf("lastLeftPercent  : %d\n", lastLeftPercent);
  Serial.printf("lastRightPercent : %d\n", lastRightPercent);
  Serial.printf("lastSeq          : 0x%02X\n", lastSeq);
  Serial.printf("lastFlags        : 0x%02X\n", lastFlags);
  Serial.printf("lastRamp         : 0x%02X\n", lastRamp);
  Serial.printf("lastDebug        : 0x%02X\n", lastDebug);
  Serial.println("================================");
  Serial.println();
}

// ============================================================
// CAN TX
// ============================================================

bool sendCan(uint32_t id, byte len, byte *buf, const char *name) {
  if (!canReady) {
    Serial.printf("[CAN] %s not sent, CAN not ready\n", name);
    return false;
  }

  byte rc = CAN0.sendMsgBuf(id, 0, len, buf);
  if (rc == CAN_OK) {
    printFrame("[CAN TX]", id, len, buf);
    return true;
  }

  Serial.printf("[CAN] %s TX failed, rc=%d\n", name, rc);
  return false;
}

void sendHeartbeat() {
  byte data[8];
  data[0] = 0xEE;
  data[1] = manualMode ? 1 : 0;
  data[2] = estopActive ? 1 : 0;
  data[3] = piAlive ? 1 : 0;
  data[4] = lastSeq;
  data[5] = (byte)((int8_t)clampValue(currentLeftCmd / 2, -128, 127));
  data[6] = (byte)((int8_t)clampValue(currentRightCmd / 2, -128, 127));
  data[7] = 0xA5;

  sendCan(CAN_TX_ID_ESP_HB, 8, data, "HEARTBEAT");
}

void sendStatus() {
  byte data[8];
  data[0] = manualMode ? 1 : 0;
  data[1] = estopActive ? 1 : 0;
  data[2] = piAlive ? 1 : 0;
  data[3] = lastSeq;
  data[4] = lastFlags;
  data[5] = lastCmd;
  data[6] = lastMode;
  data[7] = 0x55;

  sendCan(CAN_TX_ID_ESP_STATUS, 8, data, "STATUS");
}

void sendTelemetry() {
  byte data[8];
  data[0] = (byte)((int8_t)clampValue(currentLeftCmd / 2, -128, 127));
  data[1] = (byte)((int8_t)clampValue(currentRightCmd / 2, -128, 127));
  data[2] = (byte)manualSpeed;
  data[3] = lastCmd;
  data[4] = (byte)lastLeftPercent;
  data[5] = (byte)lastRightPercent;
  data[6] = lastRamp;
  data[7] = lastDebug;

  sendCan(CAN_TX_ID_ESP_TLM, 8, data, "TELEMETRY");
}

// ============================================================
// CAN RX HANDLING
// ============================================================

void handleDriveFrame(byte len, byte *buf) {
  if (len < 8) {
    Serial.println("[CAN] Drive frame too short");
    return;
  }

  lastCmd          = buf[0];
  lastMode         = buf[1];
  lastLeftPercent  = (int8_t)buf[2];
  lastRightPercent = (int8_t)buf[3];
  lastSeq          = buf[4];
  lastFlags        = buf[5];
  lastRamp         = buf[6];
  lastDebug        = buf[7];

  Serial.printf("[CAN] DRIVE RX -> cmd=0x%02X mode=0x%02X left=%d right=%d seq=0x%02X flags=0x%02X ramp=0x%02X dbg=0x%02X\n",
                lastCmd, lastMode, lastLeftPercent, lastRightPercent,
                lastSeq, lastFlags, lastRamp, lastDebug);

  lastPiSeenMs = millis();
  piAlive = true;

  if ((lastFlags & FLAG_ESTOP) || lastCmd == CMD_ESTOP) {
    activateEstop("CAN ESTOP");
    sendStatus();
    return;
  }

  if (estopActive && lastCmd == CMD_STOP) {
    clearEstop("STOP received after ESTOP");
  }

  if (manualMode) {
    Serial.println("[CAN] Ignoring CAN drive because manual mode is active");
    return;
  }

  if ((lastFlags & FLAG_ENABLE) == 0) {
    Serial.println("[CAN] ENABLE flag not set -> stop");
    stopAllMotors();
    return;
  }

  if (lastCmd == CMD_STOP) {
    setDrive(0, 0, "CAN STOP");
    return;
  }

  if (lastCmd == CMD_DRIVE) {
    int leftPwm = percentToPwm(lastLeftPercent);
    int rightPwm = percentToPwm(lastRightPercent);
    setDrive(leftPwm, rightPwm, "CAN DRIVE");
    return;
  }

  Serial.printf("[CAN] Unknown drive cmd 0x%02X\n", lastCmd);
}

void pollCan() {
  if (!canReady) return;

  while (CAN0.checkReceive() == CAN_MSGAVAIL) {
    unsigned long rxId = 0;
    byte len = 0;
    byte buf[8] = {0};

    if (CAN0.readMsgBuf(&rxId, &len, buf) != CAN_OK) {
      Serial.println("[CAN] Failed to read frame");
      return;
    }

    printFrame("[CAN RX]", rxId, len, buf);

    if (rxId == CAN_RX_ID_PI_HB) {
      lastPiSeenMs = millis();
      if (!piAlive) {
        Serial.println("[LINK] Pi heartbeat restored");
      }
      piAlive = true;
    } else if (rxId == CAN_RX_ID_DRIVE) {
      handleDriveFrame(len, buf);
    } else {
      Serial.printf("[CAN] Unhandled ID 0x%03lX\n", rxId);
    }
  }
}

void checkPiTimeout() {
  if (manualMode) return;

  if ((millis() - lastPiSeenMs) > PI_TIMEOUT_MS) {
    if (piAlive) {
      Serial.println("[LINK] Pi timeout detected -> stopping motors");
    }
    piAlive = false;
    stopAllMotors();
  }
}

// ============================================================
// SERIAL HANDLING
// ============================================================

void runSelfTest() {
  Serial.println("[SELFTEST] Forward");
  setDrive(170, 170, "SELFTEST FORWARD");
  delay(1200);
  stopAllMotors();
  delay(400);

  Serial.println("[SELFTEST] Reverse");
  setDrive(-170, -170, "SELFTEST REVERSE");
  delay(1200);
  stopAllMotors();
  delay(400);

  Serial.println("[SELFTEST] Left");
  setDrive(-170, 170, "SELFTEST LEFT");
  delay(1000);
  stopAllMotors();
  delay(400);

  Serial.println("[SELFTEST] Right");
  setDrive(170, -170, "SELFTEST RIGHT");
  delay(1000);
  stopAllMotors();
  delay(400);

  Serial.println("[SELFTEST] Done");
}

void handleSerial() {
  while (Serial.available() > 0) {
    char c = (char)Serial.read();

    if (c == '\n' || c == '\r' || c == ' ') continue;

    switch (c) {
      case 'h':
      case 'H':
        printHelp();
        break;

      case 'm':
      case 'M':
        manualMode = !manualMode;
        Serial.printf("[SERIAL] manualMode = %s\n", manualMode ? "true" : "false");
        stopAllMotors();
        sendStatus();
        break;

      case 'w':
      case 'W':
        if (manualMode) setDrive(manualSpeed, manualSpeed, "SERIAL FORWARD");
        else Serial.println("[SERIAL] Not in manual mode");
        break;

      case 's':
      case 'S':
        if (manualMode) setDrive(-manualSpeed, -manualSpeed, "SERIAL REVERSE");
        else Serial.println("[SERIAL] Not in manual mode");
        break;

      case 'a':
      case 'A':
        if (manualMode) setDrive(-manualSpeed, manualSpeed, "SERIAL LEFT");
        else Serial.println("[SERIAL] Not in manual mode");
        break;

      case 'd':
      case 'D':
        if (manualMode) setDrive(manualSpeed, -manualSpeed, "SERIAL RIGHT");
        else Serial.println("[SERIAL] Not in manual mode");
        break;

      case 'x':
      case 'X':
        stopAllMotors();
        Serial.println("[SERIAL] STOP");
        break;

      case 't':
      case 'T':
        if (manualMode) runSelfTest();
        else Serial.println("[SERIAL] Self-test allowed only in manual mode");
        break;

      case 'b':
      case 'B':
        sendHeartbeat();
        sendStatus();
        sendTelemetry();
        break;

      case 'p':
      case 'P':
        printState();
        break;

      default:
        if (c >= '0' && c <= '9') {
          int level = c - '0';
          manualSpeed = map(level, 0, 9, 0, 255);
          Serial.printf("[SERIAL] manualSpeed = %d\n", manualSpeed);
        } else {
          Serial.printf("[SERIAL] Unknown command: %c\n", c);
        }
        break;
    }
  }
}

// ============================================================
// INIT
// ============================================================

void initMotors() {
  pinMode(L_IN1, OUTPUT);
  pinMode(L_IN2, OUTPUT);
  pinMode(L_IN3, OUTPUT);
  pinMode(L_IN4, OUTPUT);

  pinMode(R_IN1, OUTPUT);
  pinMode(R_IN2, OUTPUT);
  pinMode(R_IN3, OUTPUT);
  pinMode(R_IN4, OUTPUT);

  ledcSetup(CH_L_ENA, PWM_FREQ, PWM_RES);
  ledcSetup(CH_L_ENB, PWM_FREQ, PWM_RES);
  ledcSetup(CH_R_ENA, PWM_FREQ, PWM_RES);
  ledcSetup(CH_R_ENB, PWM_FREQ, PWM_RES);

  ledcAttachPin(L_ENA, CH_L_ENA);
  ledcAttachPin(L_ENB, CH_L_ENB);
  ledcAttachPin(R_ENA, CH_R_ENA);
  ledcAttachPin(R_ENB, CH_R_ENB);

  stopAllMotors();
}

bool initCan() {
  Serial.println("[CAN] Starting SPI");
  SPI.begin(CAN_SCK_PIN, CAN_SO_PIN, CAN_SI_PIN, CAN_CS_PIN);

  pinMode(CAN_INT_PIN, INPUT);

  Serial.println("[CAN] Initializing MCP2515");
  byte rc = CAN0.begin(MCP_ANY, CAN_SPEED, CAN_CLOCK);
  if (rc != CAN_OK) {
    Serial.printf("[CAN] MCP2515 init failed, rc=%d\n", rc);
    return false;
  }

  CAN0.setMode(MCP_NORMAL);
  Serial.println("[CAN] MCP2515 set to NORMAL mode");
  return true;
}

// ============================================================
// SETUP / LOOP
// ============================================================

void setup() {
  Serial.begin(115200);
  delay(500);

  Serial.println();
  Serial.println("==================================================");
  Serial.println("ESP32 MOTOR + MCP2515 CAN CONTROLLER STARTING");
  Serial.println("==================================================");

  printHelp();

  initMotors();

  canReady = initCan();
  if (canReady) {
    Serial.println("[BOOT] CAN ready");
  } else {
    Serial.println("[BOOT] CAN init failed, manual mode still available");
  }

  sendStatus();
}

void loop() {
  handleSerial();
  pollCan();
  checkPiTimeout();

  unsigned long now = millis();

  if (now - lastHbTxMs >= HB_TX_PERIOD_MS) {
    lastHbTxMs = now;
    sendHeartbeat();
  }

  if (now - lastStatusTxMs >= STATUS_TX_PERIOD_MS) {
    lastStatusTxMs = now;
    sendStatus();
  }

  if (now - lastTlmTxMs >= TLM_TX_PERIOD_MS) {
    lastTlmTxMs = now;
    sendTelemetry();
  }
}
