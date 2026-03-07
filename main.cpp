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

SoftwareSerial BT(A3,13);

// ===== CONFIG =====
#define INVERT_X_DIR false
#define INVERT_Z_DIR true

#define X_MAX 600
#define Z_MAX 700

#define BACKOFF_STEPS 20

// ===== VELOCIDADE =====
#define STEP_DELAY_HOME_X 5000
#define STEP_DELAY_HOME_Z 6000

#define STEP_DELAY_WORK_X 2500
#define STEP_DELAY_WORK_Z 3000

// ===== SERVO =====
#define SERVO_UP 80
#define SERVO_TOUCH 0

#define SERVO_SPEED 4
#define SERVO_PRESS_TIME 60

// ===== BUFFER =====
#define CMD_BUFFER_SIZE 10

String cmdBuffer[CMD_BUFFER_SIZE];
int cmdHead=0;
int cmdTail=0;

long posX=0;
long posZ=0;

// ============================
// BUFFER

bool bufferEmpty(){
  return cmdHead==cmdTail;
}

bool bufferFull(){
  return ((cmdHead+1)%CMD_BUFFER_SIZE)==cmdTail;
}

void pushCommand(String cmd){

  if(!bufferFull()){
    cmdBuffer[cmdHead]=cmd;
    cmdHead=(cmdHead+1)%CMD_BUFFER_SIZE;
  }

}

String popCommand(){

  String cmd=cmdBuffer[cmdTail];
  cmdTail=(cmdTail+1)%CMD_BUFFER_SIZE;
  return cmd;

}

// ============================
// STEP

void stepPulse(int pin,int delayTime){

  digitalWrite(pin,HIGH);
  delayMicroseconds(delayTime);
  digitalWrite(pin,LOW);
  delayMicroseconds(delayTime);

}

// ============================
// DIREÇÃO

void setDir(int dirPin,bool dir,bool invert){

  bool finalDir=invert ? !dir : dir;
  digitalWrite(dirPin, finalDir ? HIGH : LOW);

}

// ============================
// INTERLEAVE OTIMIZADO

void moveTo(long x,long z){

  long dx=x-posX;
  long dz=z-posZ;

  bool dirX=dx>0;
  bool dirZ=dz>0;

  setDir(X_DIR,dirX,INVERT_X_DIR);
  setDir(Z_DIR,dirZ,INVERT_Z_DIR);

  long stepsX=abs(dx);
  long stepsZ=abs(dz);

  long maxSteps=max(stepsX,stepsZ);

  float incX=(float)stepsX/maxSteps;
  float incZ=(float)stepsZ/maxSteps;

  float accX=0;
  float accZ=0;

  for(long i=0;i<maxSteps;i++){

    accX+=incX;
    accZ+=incZ;

    if(accX>=1){

      if((dirX && posX<X_MAX) || (!dirX && posX>0)){
        stepPulse(X_STEP,STEP_DELAY_WORK_X);
        posX+=dirX?1:-1;
      }

      accX-=1;
    }

    if(accZ>=1){

      if((dirZ && posZ<Z_MAX) || (!dirZ && posZ>0)){
        stepPulse(Z_STEP,STEP_DELAY_WORK_Z);
        posZ+=dirZ?1:-1;
      }

      accZ-=1;
    }

  }

}

// ============================
// HOMING

void homingAxis(int stepPin,int dirPin,int limitPin,int delayTime,long &pos,bool invertDir){

  setDir(dirPin,false,invertDir);

  while(digitalRead(limitPin)==LOW){
    stepPulse(stepPin,delayTime);
  }

  setDir(dirPin,true,invertDir);

  for(int i=0;i<BACKOFF_STEPS;i++){
    stepPulse(stepPin,delayTime);
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

  int current=servoZ.read();

  if(current<target){

    for(int p=current;p<=target;p+=SERVO_SPEED){
      servoZ.write(p);
      delay(5);
    }

  }else{

    for(int p=current;p>=target;p-=SERVO_SPEED){
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
// MAPA

struct KeyMap{
  String key;
  int x;
  int z;
};

KeyMap keys[] = {

 {"ENTER",88,330},
 {"ESP",60,92},
 {"A",190,220},
 {"S",166,200},
 {"D",142,180},
 {"F",124,170},
 {"G",108,168},
 {"H",90,164},
 {"J",72,160},
 {"K",60,168},
 {"L",52,182},
 {"Z",170,176},
 {"X",144,156},
 {"C",123,140},
 {"V",100,130},
 {"B",80,128},
 {"N",66,133},
 {"M",46,128},
 {"I",89,210},
 {"O",86,228},
 {"P",86,250},
 {"Y",114,201},
 {"U",100,200},
 {"T",128,199},
 {"R",146,214},
 {"E",171,233},
 {"W",199,268},
 {"Q",236,319}

};

int keyCount=sizeof(keys)/sizeof(keys[0]);

// ============================

void typeKey(String k){

  for(int i=0;i<keyCount;i++){

    if(keys[i].key==k){
      moveTo(keys[i].x,keys[i].z);
      pressKey();
      return;
    }

  }

}

void typeText(String text){

  text.toUpperCase();

  for(int i=0;i<text.length();i++){

    char c=text.charAt(i);

    if(c==' '){
      typeKey("ESP");
      continue;
    }

    typeKey(String(c));

  }

  // ENTER AUTOMÁTICO APÓS O TEXTO
  typeKey("ENTER");

}

// ============================
// BENCHMARK

void benchmarkTest(){

  Serial.println("BENCHMARK iniciado");

  unsigned long startTime=millis();

  int totalKeys=0;

  String test1="QWERTYUIOP";
  String test2="ASDFGHJKL";
  String test3="ZXCVBNM";

  typeText(test1);
  totalKeys+=test1.length()+1;

  typeText(test2);
  totalKeys+=test2.length()+1;

  typeText(test3);
  totalKeys+=test3.length()+1;

  unsigned long totalTime=millis()-startTime;

  float keysPerSecond=(float)totalKeys/(totalTime/1000.0);
  float keysPerMinute=keysPerSecond*60.0;

  Serial.print("Teclas digitadas: ");
  Serial.println(totalKeys);

  Serial.print("Tempo total (ms): ");
  Serial.println(totalTime);

  Serial.print("Teclas/segundo: ");
  Serial.println(keysPerSecond);

  Serial.print("Teclas/minuto: ");
  Serial.println(keysPerMinute);

  Serial.println("BENCHMARK finalizado");

}

// ============================
// EXECUTAR COMANDOS

void executeGcode(String line){

  line.trim();
  line.toUpperCase();

  if(line.startsWith("TYPE ")){
    typeText(line.substring(5));
  }

  else if(line=="BENCHMARK"){
    benchmarkTest();
  }

  else if(line=="RESTART"){
    homing();
  }

  else if(line.startsWith("G1")){

    int xi=line.indexOf('X');
    int zi=line.indexOf('Z');

    long newX=posX;
    long newZ=posZ;

    if(xi>=0)newX=line.substring(xi+1).toInt();
    if(zi>=0)newZ=line.substring(zi+1).toInt();

    moveTo(newX,newZ);
  }

  else if(line=="CLICK"){
    pressKey();
  }

  Serial.println("OK");
  BT.println("OK");
}

// ============================

void setup(){

  Serial.begin(115200);
  BT.begin(9600);

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
    pushCommand(Serial.readStringUntil('\n'));
  }

  if(BT.available()){
    pushCommand(BT.readStringUntil('\n'));
  }

  if(!bufferEmpty()){
    executeGcode(popCommand());
  }

}
