#include <Servo.h>
#include <SoftwareSerial.h>

Servo servoZ;

// ===== PINOS =====
#define X_STEP 2
#define X_DIR 5
#define X_LIMIT 9

#define Z_STEP 4
#define Z_DIR 7
#define Z_LIMIT 11

#define SERVO_PIN 12

SoftwareSerial BT(A3, 13);

// ===== CONFIG =====
#define INVERT_X_DIR false
#define INVERT_Z_DIR true

#define X_MAX 600
#define Z_MAX 700

#define BACKOFF_STEPS 20

// ===== VELOCIDADE =====
#define STEP_DELAY_HOME_X 9500
#define STEP_DELAY_HOME_Z 6000

#define STEP_DELAY_WORK_X 2500
#define STEP_DELAY_WORK_Z 3000

// ===== SERVO =====
#define SERVO_UP 80
#define SERVO_TOUCH 0

#define SERVO_SPEED 4
#define SERVO_PRESS_TIME 60

// ===== BUFFER (Reduzido para economizar RAM) =====
#define CMD_BUFFER_SIZE 5

String cmdBuffer[CMD_BUFFER_SIZE];
int cmdHead = 0;
int cmdTail = 0;

long posX = 0;
long posZ = 0;

bool repeatRunning = false;

// ============================
// BUFFER
bool bufferEmpty() { return cmdHead == cmdTail; }
bool bufferFull() { return ((cmdHead + 1) % CMD_BUFFER_SIZE) == cmdTail; }

void pushCommand(String cmd) {
  if (!bufferFull()) {
    cmdBuffer[cmdHead] = cmd;
    cmdHead = (cmdHead + 1) % CMD_BUFFER_SIZE;
  }
}

String popCommand() {
  String cmd = cmdBuffer[cmdTail];
  cmdTail = (cmdTail + 1) % CMD_BUFFER_SIZE;
  return cmd;
}

// ============================
// STEP / DIR
void stepPulse(int pin, int delayTime) {
  digitalWrite(pin, HIGH); delayMicroseconds(delayTime);
  digitalWrite(pin, LOW);  delayMicroseconds(delayTime);
}

void setDir(int dirPin, bool dir, bool invert) {
  bool finalDir = invert ? !dir : dir;
  digitalWrite(dirPin, finalDir ? HIGH : LOW);
}

// ============================
// MOVE
void moveTo(long x, long z) {
  long dx = x - posX; long dz = z - posZ;
  bool dirX = dx > 0; bool dirZ = dz > 0;
  setDir(X_DIR, dirX, INVERT_X_DIR);
  setDir(Z_DIR, dirZ, INVERT_Z_DIR);
  long stepsX = abs(dx); long stepsZ = abs(dz);
  long maxSteps = max(stepsX, stepsZ);
  float incX = (float)stepsX / maxSteps;
  float incZ = (float)stepsZ / maxSteps;
  float accX = 0; float accZ = 0;
  for (long i = 0; i < maxSteps; i++) {
    accX += incX; accZ += incZ;
    if (accX >= 1) {
      if ((dirX && posX < X_MAX) || (!dirX && posX > 0)) {
        stepPulse(X_STEP, STEP_DELAY_WORK_X); posX += dirX ? 1 : -1;
      }
      accX -= 1;
    }
    if (accZ >= 1) {
      if ((dirZ && posZ < Z_MAX) || (!dirZ && posZ > 0)) {
        stepPulse(Z_STEP, STEP_DELAY_WORK_Z); posZ += dirZ ? 1 : -1;
      }
      accZ -= 1;
    }
  }
}

// ============================
// HOMING
void homingAxis(int stepPin, int dirPin, int limitPin, int delayTime, long &pos, bool invertDir) {
  setDir(dirPin, false, invertDir);
  while (digitalRead(limitPin) == LOW) { stepPulse(stepPin, delayTime); }
  setDir(dirPin, true, invertDir);
  for (int i = 0; i < BACKOFF_STEPS; i++) { stepPulse(stepPin, delayTime); }
  pos = 0;
}

void homing() {
  Serial.println(F("Homing inicial"));
  homingAxis(X_STEP, X_DIR, X_LIMIT, STEP_DELAY_HOME_X, posX, INVERT_X_DIR);
  delay(200);
  homingAxis(Z_STEP, Z_DIR, Z_LIMIT, STEP_DELAY_HOME_Z, posZ, INVERT_Z_DIR);
  Serial.println(F("Zero definido"));
}

// ============================
// SERVO
void servoMoveSmooth(int target) {
  int current = servoZ.read();
  if (current < target) {
    for (int p = current; p <= target; p += SERVO_SPEED) { servoZ.write(p); delay(5); }
  } else {
    for (int p = current; p >= target; p -= SERVO_SPEED) { servoZ.write(p); delay(5); }
  }
}

void pressKey() {
  servoMoveSmooth(SERVO_TOUCH);
  delay(SERVO_PRESS_TIME);
  servoMoveSmooth(SERVO_UP);
}

// ============================
// MAPA DE TECLAS
struct KeyMap {
  char key[6];
  int x;
  int z;
};

KeyMap keys[] = {
  {"ENTER", 205, 230}, {"ESP", 120, 232}, {"A", 34, 184}, {"S", 39, 170},
  {"D", 46, 160}, {"F", 54, 148}, {"G", 70, 147}, {"H", 82, 142},
  {"J", 102, 149}, {"K", 120, 155}, {"L", 140, 167}, {"Z", 62, 217},
  {"X", 65, 202}, {"C", 71, 190}, {"V", 81, 184}, {"B", 93, 178},
  {"N", 109, 182}, {"M", 126, 187}, {"P", 141, 144}, {"O", 120, 132},
  {"I", 100, 120}, {"U", 80, 112}, {"Y", 62, 114}, {"T", 46, 116},
  {"R", 30, 120}, {"E", 22, 133}, {"W", 16, 145}, {"Q", 13, 164},
  {"TIL", 176, 197}, {"ACU", 162, 156}, {"CED", 159, 181}, {"CIR", 0, 0}, {"GRV", 0, 0}
};

int keyCount = sizeof(keys) / sizeof(keys[0]);

struct ComboKey {
  char trigger[4];
  char key1[6];
  char key2[6];
};

ComboKey combos[] = {
  {"\xC3\xA3", "TIL", "A"}, {"\xC3\x83", "TIL", "A"}, {"\xC3\xB5", "TIL", "O"}, {"\xC3\x95", "TIL", "O"},
  {"\xC3\xA1", "ACU", "A"}, {"\xC3\x81", "ACU", "A"}, {"\xC3\xA9", "ACU", "E"}, {"\xC3\x89", "ACU", "E"},
  {"\xC3\xAD", "ACU", "I"}, {"\xC3\x8D", "ACU", "I"}, {"\xC3\xB3", "ACU", "O"}, {"\xC3\x93", "ACU", "O"},
  {"\xC3\xBA", "ACU", "U"}, {"\xC3\x9A", "ACU", "U"}, {"\xC3\xA2", "CIR", "A"}, {"\xC3\x82", "CIR", "A"},
  {"\xC3\xAA", "CIR", "E"}, {"\xC3\x8A", "CIR", "E"}, {"\xC3\xB4", "CIR", "O"}, {"\xC3\x94", "CIR", "O"},
  {"\xC3\xA0", "GRV", "A"}, {"\xC3\x80", "GRV", "A"}, {"\xC3\xA7", "CED", ""}, {"\xC3\x87", "CED", ""}
};

int comboCount = sizeof(combos) / sizeof(combos[0]);

int findCombo(const String& text, int i) {
  int len = text.length();
  for (int c = 0; c < comboCount; c++) {
    int tlen = strlen(combos[c].trigger);
    if (i + tlen > len) continue;
    bool match = true;
    for (int j = 0; j < tlen; j++) {
      if ((uint8_t)text.charAt(i + j) != (uint8_t)combos[c].trigger[j]) {
        match = false; break;
      }
    }
    if (match) return c;
  }
  return -1;
}

// ============================
// JOG MANUAL
void jogAxis(char axis, long steps) {
  bool dir = steps > 0; long count = abs(steps);
  int stepP = (axis == 'X') ? X_STEP : Z_STEP;
  int dirP  = (axis == 'X') ? X_DIR : Z_DIR;
  bool inv  = (axis == 'X') ? INVERT_X_DIR : INVERT_Z_DIR;
  long maxP = (axis == 'X') ? X_MAX : Z_MAX;
  long &curP = (axis == 'X') ? posX : posZ;
  int dly   = (axis == 'X') ? STEP_DELAY_WORK_X : STEP_DELAY_WORK_Z;

  setDir(dirP, dir, inv);
  for (long i = 0; i < count; i++) {
    if ((dir && curP < maxP) || (!dir && curP > 0)) {
      stepPulse(stepP, dly); curP += dir ? 1 : -1;
    }
  }
}

void printPos() {
  Serial.print(F("POS X=")); Serial.print(posX);
  Serial.print(F(" Z=")); Serial.println(posZ);
  BT.print(F("POS X=")); BT.print(posX);
  BT.print(F(" Z=")); BT.println(posZ);
}

void printSetKey(String keyName) {
  keyName.toUpperCase();
  Serial.print(F("  {\"")); Serial.print(keyName); Serial.print(F("\","));
  Serial.print(posX); Serial.print(F(",")); Serial.print(posZ); Serial.println(F("},"));
}

void handleJog(String line) {
  String param = line.substring(4); param.trim();
  if (param.length() < 2) { Serial.println(F("ERR JOG")); return; }
  char axis = param.charAt(0);
  char sign = param.charAt(1);
  long steps = param.substring(2).toInt();
  if (sign == '-') steps = -steps;
  jogAxis(axis, steps);
  printPos();
}

// ============================
// REPEAT
bool waitWithStop(unsigned long ms) {
  unsigned long start = millis();
  while (millis() - start < ms) {
    if (Serial.available() || BT.available()) {
      String inc = Serial.available() ? Serial.readStringUntil('\n') : BT.readStringUntil('\n');
      inc.trim(); inc.toUpperCase();
      if (inc == F("STOP")) { 
        Serial.println(F("STOP - cancelado")); 
        BT.println(F("STOP")); 
        return false; 
      }
    }
  }
  return true;
}

void typeKey(const char* k) {
  for (int i = 0; i < keyCount; i++) {
    if (strcmp(keys[i].key, k) == 0) {
      moveTo(keys[i].x, keys[i].z);
      pressKey();
      return;
    }
  }
}

void typeText(String text) {
  int i = 0;
  while (i < (int)text.length()) {
    char c = text.charAt(i);
    if (c == ' ') { typeKey("ESP"); i++; continue; }
    int ci = findCombo(text, i);
    if (ci >= 0) {
      typeKey(combos[ci].key1);
      if (combos[ci].key2[0] != '\0') typeKey(combos[ci].key2);
      i += strlen(combos[ci].trigger);
      continue;
    }
    char upper = (c >= 'a' && c <= 'z') ? c - 32 : c;
    char buf[2] = {upper, '\0'};
    typeKey(buf);
    i++;
  }
  typeKey("ENTER");
}

void handleRepeat(String& cmd, String& orig) {
  int i1 = cmd.indexOf(' ');
  int i2 = cmd.indexOf(' ', i1 + 1);
  int i3 = cmd.indexOf(' ', i2 + 1);
  if (i1 < 0 || i2 < 0 || i3 < 0) return;

  int vezes = cmd.substring(i1 + 1, i2).toInt();
  int intervaloSeg = cmd.substring(i2 + 1, i3).toInt();
  String texto = orig.substring(i3 + 1); texto.trim();

  repeatRunning = true;
  for (int i = 1; i <= vezes && repeatRunning; i++) {
    BT.print(F("[")); BT.print(i); BT.print(F("/")); BT.print(vezes); BT.println(F("]"));
    typeText(texto);
    if (i < vezes && !waitWithStop((unsigned long)intervaloSeg * 1000UL)) repeatRunning = false;
  }
  repeatRunning = false;
}

// ============================
// EXECUÇÃO
void executeGcode(String line) {
  line.trim();
  String original = line;
  line.toUpperCase();

  if (line.startsWith(F("TYPE "))) {
    typeText(original.substring(5));
  } else if (line.startsWith(F("REPEAT "))) {
    handleRepeat(line, original);
  } else if (line == F("STOP")) {
    repeatRunning = false;
  } else if (line == F("RESTART")) {
    homing();
  } else if (line.startsWith(F("G1"))) {
    int xi = line.indexOf('X'), zi = line.indexOf('Z');
    long nX = (xi >= 0) ? line.substring(xi + 1).toInt() : posX;
    long nZ = (zi >= 0) ? line.substring(zi + 1).toInt() : posZ;
    moveTo(nX, nZ);
  } else if (line == F("CLICK")) {
    pressKey();
  } else if (line.startsWith(F("JOG "))) {
    handleJog(line);
  } else if (line == F("POS")) {
    printPos();
  } else if (line.startsWith(F("SETKEY "))) {
    printSetKey(line.substring(7));
  }
  Serial.println(F("OK")); BT.println(F("OK"));
}

void setup() {
  Serial.begin(115200); BT.begin(9600);
  servoZ.attach(SERVO_PIN);
  pinMode(X_STEP, OUTPUT); pinMode(X_DIR, OUTPUT); pinMode(X_LIMIT, INPUT_PULLUP);
  pinMode(Z_STEP, OUTPUT); pinMode(Z_DIR, OUTPUT); pinMode(Z_LIMIT, INPUT_PULLUP);
  servoZ.write(SERVO_UP);
  delay(2000);
  homing();
  Serial.println(F("CNC Keyboard pronta"));
}

void loop() {
  if (Serial.available()) pushCommand(Serial.readStringUntil('\n'));
  if (BT.available()) pushCommand(BT.readStringUntil('\n'));
  if (!bufferEmpty()) executeGcode(popCommand());
}
