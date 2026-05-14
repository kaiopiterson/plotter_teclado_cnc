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
#define STEP_DELAY_HOME_X 9500
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

// ===== REPEAT STATE =====
bool repeatRunning=false;

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
// FIX: char key[6] em vez de String — elimina 28 alocações heap
//      que causavam fragmentação após ~18 repetições

struct KeyMap{
  char key[6];
  int x;
  int z;
};

KeyMap keys[] = {
 {"ENTER",205,230},
 {"ESP",120,232},
 {"A",34,182},
 {"S",39,170},
 {"D",46,160},
 {"F",54,148},
 {"G",70,147},
 {"H",82,142},
 {"J",102,149},
 {"K",120,155},
 {"L",140,167},
 {"Z",62,217},
 {"X",65,202},
 {"C",71,190},
 {"V",81,184},
 {"B",93,178},
 {"N",109,180},
 {"M",126,187},
 {"P",141,144},
 {"O",120,132},
 {"I",100,120},
 {"U",80,112},
 {"Y",62,114},
 {"T",46,116},
 {"R",30,120},
 {"E",22,133},
 {"W",16,145},
 {"Q",13,164},
 {",",13,164},
 {".",13,164},
 {"Ç",13,164},
 {"Ã",13,164},
 {"Õ",13,164},
 {"Í",13,164},
 {"Á",13,164},
 {"Ó",13,164}
};

int keyCount=sizeof(keys)/sizeof(keys[0]);

// ============================
// JOG MANUAL

void jogAxis(char axis,long steps){

  if(axis=='X'){
    bool dir=steps>0;
    long count=abs(steps);
    setDir(X_DIR,dir,INVERT_X_DIR);
    for(long i=0;i<count;i++){
      if((dir && posX<X_MAX) || (!dir && posX>0)){
        stepPulse(X_STEP,STEP_DELAY_WORK_X);
        posX+=dir?1:-1;
      }
    }
  }

  else if(axis=='Z'){
    bool dir=steps>0;
    long count=abs(steps);
    setDir(Z_DIR,dir,INVERT_Z_DIR);
    for(long i=0;i<count;i++){
      if((dir && posZ<Z_MAX) || (!dir && posZ>0)){
        stepPulse(Z_STEP,STEP_DELAY_WORK_Z);
        posZ+=dir?1:-1;
      }
    }
  }

}

void printPos(){
  String msg="POS X="+String(posX)+" Z="+String(posZ);
  Serial.println(msg);
  BT.println(msg);
}

void printSetKey(String keyName){
  keyName.toUpperCase();
  String line="  {\""+keyName+"\","+String(posX)+","+String(posZ)+"},";
  Serial.println(line);
  BT.println(line);
}

void handleJog(String line){

  String param=line.substring(4);
  param.trim();

  if(param.length()<2){
    Serial.println("ERR JOG: use JOG X+N ou JOG Z-N");
    return;
  }

  char axis=param.charAt(0);
  char sign=param.charAt(1);
  long steps=param.substring(2).toInt();

  if(sign=='-') steps=-steps;

  jogAxis(axis,steps);
  printPos();

}

// ============================
// REPEAT

bool waitWithStop(unsigned long ms){

  unsigned long elapsed=0;
  const unsigned long chunk=100;

  while(elapsed<ms){

    unsigned long t=min(chunk, ms-elapsed);
    delay(t);
    elapsed+=t;

    if(Serial.available()){
      String incoming=Serial.readStringUntil('\n');
      incoming.trim();
      incoming.toUpperCase();
      if(incoming=="STOP"){
        Serial.println("STOP recebido — cancelado");
        BT.println("STOP recebido");
        return false;
      }
    }

    if(BT.available()){
      String incoming=BT.readStringUntil('\n');
      incoming.trim();
      incoming.toUpperCase();
      if(incoming=="STOP"){
        Serial.println("STOP recebido — cancelado");
        BT.println("STOP recebido");
        return false;
      }
    }

  }

  return true;

}

void handleRepeat(String line){

  // Localiza os 3 espaços delimitadores sem reassinar a String
  // preservando os espaços internos do texto
  int i1=line.indexOf(' ');
  int i2=line.indexOf(' ', i1+1);
  int i3=line.indexOf(' ', i2+1);

  if(i1<0 || i2<0 || i3<0){
    Serial.println("ERR REPEAT: REPEAT <vezes> <intervalo_s> <texto>");
    return;
  }

  int vezes        = line.substring(i1+1, i2).toInt();
  int intervaloSeg = line.substring(i2+1, i3).toInt();
  String texto     = line.substring(i3+1);
  texto.trim();

  if(vezes<=0 || intervaloSeg<0 || texto.length()==0){
    Serial.println("ERR REPEAT: parametros invalidos");
    return;
  }

  Serial.print("REPEAT iniciado: ");
  Serial.print(vezes);
  Serial.print("x | intervalo: ");
  Serial.print(intervaloSeg);
  Serial.print("s | texto: [");
  Serial.print(texto);
  Serial.println("]");
  BT.println("REPEAT iniciado. STOP para cancelar.");

  repeatRunning=true;

  for(int i=1;i<=vezes;i++){

    if(!repeatRunning) break;

    Serial.print("[");
    Serial.print(i);
    Serial.print("/");
    Serial.print(vezes);
    Serial.println("]");

    typeText(texto);

    if(i<vezes){
      Serial.print("Aguardando ");
      Serial.print(intervaloSeg);
      Serial.println("s...");

      bool continua=waitWithStop((unsigned long)intervaloSeg*1000UL);
      if(!continua){
        repeatRunning=false;
        break;
      }
    }

  }

  if(repeatRunning){
    Serial.println("REPEAT concluido.");
    BT.println("REPEAT concluido.");
  }

  repeatRunning=false;

}

// ============================
// TYPE KEY
// FIX: recebe const char* e usa strcmp — zero alocação heap

void typeKey(const char* k){
  for(int i=0;i<keyCount;i++){
    if(strcmp(keys[i].key, k)==0){
      moveTo(keys[i].x,keys[i].z);
      pressKey();
      return;
    }
  }
}

// ============================
// TYPE TEXT
// FIX: usa buf[2] na stack em vez de String(c) no heap

void typeText(String text){

  text.toUpperCase();

  for(int i=0;i<text.length();i++){

    char c=text.charAt(i);

    if(c==' '){
      typeKey("ESP");
      continue;
    }

    // buf na stack — sem alocação heap, sem fragmentação
    char buf[2];
    buf[0]=c;
    buf[1]='\0';
    typeKey(buf);

  }

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

  typeText(test1); totalKeys+=test1.length()+1;
  typeText(test2); totalKeys+=test2.length()+1;
  typeText(test3); totalKeys+=test3.length()+1;

  unsigned long totalTime=millis()-startTime;
  float keysPerSecond=(float)totalKeys/(totalTime/1000.0);
  float keysPerMinute=keysPerSecond*60.0;

  Serial.print("Teclas digitadas: "); Serial.println(totalKeys);
  Serial.print("Tempo total (ms): "); Serial.println(totalTime);
  Serial.print("Teclas/segundo: ");   Serial.println(keysPerSecond);
  Serial.print("Teclas/minuto: ");    Serial.println(keysPerMinute);
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

  else if(line.startsWith("REPEAT ")){
    handleRepeat(line);
    return;
  }

  else if(line=="STOP"){
    repeatRunning=false;
    Serial.println("STOP recebido");
    BT.println("STOP recebido");
    return;
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

    if(xi>=0) newX=line.substring(xi+1).toInt();
    if(zi>=0) newZ=line.substring(zi+1).toInt();

    moveTo(newX,newZ);
  }

  else if(line=="CLICK"){
    pressKey();
  }

  else if(line.startsWith("JOG ")){
    handleJog(line);
    return;
  }

  else if(line=="POS"){
    printPos();
    return;
  }

  else if(line.startsWith("SETKEY ")){
    printSetKey(line.substring(7));
    return;
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
  Serial.println("Comandos: TYPE / REPEAT / STOP / JOG / POS / SETKEY / BENCHMARK / RESTART");

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
