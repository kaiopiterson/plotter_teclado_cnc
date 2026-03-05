# plotter_teclado_cnc

CNC Keyboard Plotter

Firmware para um CNC Plotter de Teclado, baseado em Arduino, capaz de posicionar um atuador sobre teclas físicas e pressioná-las automaticamente.

O sistema utiliza dois eixos de movimento (X e Z) controlados por motores de passo e um servo motor para pressionar teclas mapeadas por coordenadas.

Esse projeto permite automatizar a digitação de sequências de teclas em um teclado físico.

Visão Geral

O firmware implementa um pequeno sistema de controle CNC especializado para interação com teclados.

Principais capacidades:

controle de dois eixos CNC

homing automático com fim de curso

servo para pressionar teclas

mapeamento de teclas por coordenadas

execução de sequências de digitação

repetição automática de comandos

controle via Serial Monitor

Arquitetura do Sistema
Eixos

O sistema possui dois eixos:

Eixo	Função
X	movimento horizontal
Z	movimento vertical

Cada eixo é controlado por um driver de motor de passo com sinais:

STEP
DIR
Servo

O servo motor é responsável por pressionar a tecla.

Fluxo de operação:

mover para posição da tecla

descer o servo

pressionar

subir o servo

Parâmetros configuráveis:

SERVO_UP
SERVO_TOUCH
SERVO_SPEED
SERVO_PRESS_TIME
Velocidades de Movimento

O firmware utiliza duas velocidades distintas.

Homing (lento / alto torque)

Utilizado para encontrar o fim de curso com segurança.

STEP_DELAY_HOME_X
STEP_DELAY_HOME_Z
Digitação (rápido)

Utilizado para deslocamento entre teclas.

STEP_DELAY_WORK_X
STEP_DELAY_WORK_Z

Essa separação melhora:

segurança no homing

velocidade na digitação

Homing

Durante a inicialização o sistema executa homing automático.

Fluxo:

mover eixo até acionar fim de curso

recuar alguns passos

definir posição como (0,0)

Isso garante alinhamento entre posição lógica e posição física.

Mapeamento de Teclas

Cada tecla é definida por uma estrutura:

{"TECLA", X, Z}

Exemplo:

{"K",46,130}
{"A",190,198}
{"ENTER",75,330}

Essas coordenadas correspondem à posição física da tecla no teclado.

Comandos Serial

O sistema é controlado via Serial Monitor.

Velocidade padrão:

115200
Movimento manual

Move o cabeçote para uma posição específica.

G1 X100 Z200
Teste de clique

Pressiona a tecla na posição atual.

CLICK
Digitação

Sequência de teclas separadas por espaço:

Q W E R T Y
Loop de digitação

Executa continuamente a sequência.

Q W E R T Y LOOP
Parar loop

Interrompe o loop e retorna para posição segura.

PAUSE

Posição segura:

X20 Z20
Exemplo de Uso

Sequência simples:

Q W E R T Y ESP K A I O ESP T Y P E ENTER

Loop contínuo:

Q W E R T Y ESP K A I O ESP T Y P E ENTER LOOP
Hardware Utilizado

Exemplo de componentes:

Arduino Uno / Nano

CNC Shield ou drivers A4988 / DRV8825

2x motores NEMA 17

servo SG90

2x sensores de fim de curso

estrutura mecânica XY

Estrutura do Firmware

Principais módulos do código:

STEP CONTROL
AXIS MOVEMENT
HOMING
SERVO CONTROL
KEYBOARD MAPPING
SERIAL COMMAND PARSER
Segurança

O firmware inclui proteção básica:

limite máximo de movimento

controle de posição lógica

homing automático

recuo após fim de curso

Possíveis Melhorias

Evoluções futuras do projeto:

movimento simultâneo X/Z (interpolação CNC)

buffer de comandos

digitação de texto completo

otimização de trajetórias

aumento da velocidade de digitação

interface via USB HID ou API

Licença

Projeto aberto para experimentação e uso educacional.