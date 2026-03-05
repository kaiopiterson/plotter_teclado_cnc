#include <Servo.h>

Servo servoZ;

// ===== PINOS =====
#define X_STEP 2
#define X_DIR 5
#define X_LIMIT 9

#define Z_STEP 4
#define Z_DIR 7
#define Z_LIMIT 11

#define SERVO_PIN 12

// ===== CONFIG =====
#define INVERT_X_DIR false
#define INVERT_Z_DIR true

#define X_MAX 600
#define Z_MAX 700

#define BACKOFF_STEPS 20

// ===== VELOCIDADE HOMING (TORQUE) =====
#define STEP_DELAY_HOME_X 5000
#define STEP_DELAY_HOME_Z 6000

// ===== VELOCIDADE DIGITAÇÃO =====
#define STEP_DELAY_WORK_X 2500
#define STEP_DELAY_WORK_Z 3000

// ===== SERVO CONFIG =====
#define SERVO_UP 80
#define SERVO_TOUCH 0

#define SERVO_SPEED 4
#define SERVO_PRESS_TIME 60

long posX = 0;
long posZ = 0;

// ===== CONTROLE DE LOOP =====
bool loopActive = false;
String loopCommand = "";

// ============================
// STEP

void stepOnce(int stepPin,int delayTime){

  digitalWrite(stepPin,HIGH);
  delayMicroseconds(delayTime);

  digitalWrite(stepPin,LOW);
  delayMicroseconds(delayTime);

}

// ============================
// DIREÇÃO

void setDir(int dirPin,bool dir,bool invert){

  bool finalDir = invert ? !dir : dir;

  digitalWrite(dirPin, finalDir ? HIGH : LOW);

}

// ============================
// MOVIMENTO

void moveAxis(long target,
              int stepPin,
              int dirPin,
              int delayTime,
              long &pos,
              long maxTravel,
              bool invertDir){

  long steps = target-pos;

  bool dir = steps>0;

  setDir(dirPin,dir,invertDir);

  long total=abs(steps);

  for(long i=0;i<total;i++){

    if(dir && pos>=maxTravel) return;
    if(!dir && pos<=0) return;

    stepOnce(stepPin,delayTime);

    pos += dir ? 1 : -1;
  }

}

// ============================

void moveTo(long x,long z){

  moveAxis(x,
           X_STEP,X_DIR,
           STEP_DELAY_WORK_X,
           posX,
           X_MAX,
           INVERT_X_DIR);

  moveAxis(z,
           Z_STEP,Z_DIR,
           STEP_DELAY_WORK_Z,
           posZ,
           Z_MAX,
           INVERT_Z_DIR);

}

// ============================
// HOMING

void homingAxis(int stepPin,
                int dirPin,
                int limitPin,
                int delayTime,
                long &pos,
                bool invertDir){

  setDir(dirPin,false,invertDir);

  while(digitalRead(limitPin)==LOW){
    stepOnce(stepPin,delayTime);
  }

  setDir(dirPin,true,invertDir);

  for(int i=0;i<BACKOFF_STEPS;i++){
    stepOnce(stepPin,delayTime);
  }

  pos=0;

}

void homing(){

  Serial.println("Homing inicial");

  homingAxis(X_STEP,X_DIR,X_LIMIT,STEP_DELAY_HOME_X,posX,INVERT_X_DIR);

  delay(200);

  homingAxis(Z_STEP,Z_DIR,Z_LIMIT,STEP_DELAY_HOME_Z,posZ,INVERT_Z_DIR);

  Serial.println("Zero definido");

}

// ============================
// SERVO

void servoMoveSmooth(int target){

  int current = servoZ.read();

  if(current < target){

    for(int p=current; p<=target; p+=SERVO_SPEED){
      servoZ.write(p);
      delay(5);
    }

  }
  else{

    for(int p=current; p>=target; p-=SERVO_SPEED){
      servoZ.write(p);
      delay(5);
    }

  }

}

void pressKey(){

  servoMoveSmooth(SERVO_TOUCH);

  delay(SERVO_PRESS_TIME);

  servoMoveSmooth(SERVO_UP);

}

// ============================
// MAPA TECLADO

struct KeyMap{

  String key;
  int x;
  int z;

};

KeyMap keys[] = {

 {"ENTER",75,330},
 {"ESP",30,72},
 {"K",46,130},
 {"A",190,198},
 {"S",165,173},
 {"I",65,165},
 {"O",60,180},
 {"U",80,165},
 {"Y",96,165},
 {"P",55,198},
 {"T",116,170},
 {"R",140,182},
 {"E",160,198},
 {"W",182,219},
 {"Q",212,260}

};

int keyCount = sizeof(keys)/sizeof(keys[0]);

// ============================

void typeKey(String k){

  for(int i=0;i<keyCount;i++){

    if(keys[i].key==k){

      moveTo(keys[i].x,keys[i].z);

      pressKey();

      return;

    }

  }

  Serial.print("Tecla nao mapeada: ");
  Serial.println(k);

}

// ============================
// EXECUTAR SEQUENCIA

void executeSequence(String line){

  line.toUpperCase();

  int start = 0;

  while(true){

    int spaceIndex = line.indexOf(' ', start);

    String word;

    if(spaceIndex == -1)
      word = line.substring(start);
    else
      word = line.substring(start, spaceIndex);

    word.trim();

    if(word.length() > 0)
      typeKey(word);

    if(spaceIndex == -1)
      break;

    start = spaceIndex + 1;

  }

}

// ============================
// EXECUTAR COMANDOS

void executeGcode(String line){

  line.trim();
  line.toUpperCase();

  if(line.startsWith("G0") || line.startsWith("G1")){

    int xi = line.indexOf('X');
    int zi = line.indexOf('Z');

    long newX = posX;
    long newZ = posZ;

    if(xi>=0) newX = line.substring(xi+1).toInt();
    if(zi>=0) newZ = line.substring(zi+1).toInt();

    moveTo(newX,newZ);

    Serial.print("POS X=");
    Serial.print(posX);
    Serial.print(" Z=");
    Serial.println(posZ);

    return;

  }

  if(line == "CLICK"){

    pressKey();

    Serial.println("CLICK executado");

    return;

  }

  if(line.endsWith("LOOP")){

    loopCommand = line.substring(0,line.length()-4);

    loopCommand.trim();

    loopActive = true;

    Serial.println("LOOP iniciado");

    return;

  }

  if(line == "PAUSE"){

    loopActive = false;

    Serial.println("LOOP parado");

    moveTo(20,20);

    return;

  }

  executeSequence(line);

}

// ============================

void setup(){

  Serial.begin(115200);

  servoZ.attach(SERVO_PIN);

  pinMode(X_STEP,OUTPUT);
  pinMode(X_DIR,OUTPUT);
  pinMode(X_LIMIT,INPUT_PULLUP);

  pinMode(Z_STEP,OUTPUT);
  pinMode(Z_DIR,OUTPUT);
  pinMode(Z_LIMIT,INPUT_PULLUP);

  servoZ.write(SERVO_UP);

  delay(2000);

  homing();

  Serial.println("CNC Keyboard pronta");

}

// ============================

void loop(){

  if(Serial.available()){

    String line = Serial.readStringUntil('\n');

    executeGcode(line);

  }

  if(loopActive){

    executeSequence(loopCommand);

    delay(300);

  }

}