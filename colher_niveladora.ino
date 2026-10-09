//  COLHER NIVELADORA - (Arduino UNO Q)
//
//  O que faz, 100 vezes por segundo:
//   1) Le o MPU6050 (preso no CABO) e calcula a inclinacao do cabo.
//   2) Calcula quanto o servo precisa girar para a COLHER ficar
//      no angulo desejado (o "alvo"), mesmo com o cabo inclinado.
//   3) Move o servo de forma suave.
//
//  AO LIGAR: deixe a colher PARADA, apoiada na mesa, por uns 5 segundos.

#include <Arduino_RouterBridge.h>
#include <Wire.h>
#include <Servo.h>

// AJUSTES 
const int   SINAL         = 1;      // se a colher girar para o lado ERRADO, troque para -1
const int   US_CENTRO     = 1500;   
const float US_POR_GRAU   = 10.0;   

const bool  USAR_POTS     = true;   // false = ignora os potenciometros e usa os valores fixos abaixo
const float ALVO_FIXO     = 0.0;   
const float PASSO_FIXO    = 3.0;    

const int   PINO_SERVO     = 9;
const int   PINO_POT_ALVO  = A2;
const int   PINO_POT_SUAVE = A3;
const float ALVO_MAX      = 20.0;   
const float PASSO_MINIMO  = 1.0;    
const float PASSO_MAXIMO  = 5.0;    

const float LIMITE        = 45.0;   
const float ZONA_MORTA    = .0;    
const float ANG_SEGURANCA = 70.0;   
const float ALFA          = 0.98;   
const unsigned long TEMPO_SEM_SENSOR = 300;   

const uint8_t ENDERECO_MPU = 0x68;

Servo servo;
TwoWire *barramento = &Wire;        

float biasGy = 0;                  
float angulo = 0;                  
float alvo = ALVO_FIXO;            
float passo = PASSO_FIXO;         
float comando = 0;               
float comandoEnviado = 0;        
bool sensorOk = false;
unsigned long tAnterior = 0;     
unsigned long tUltimaBoa = 0;     
unsigned long tTela = 0;           
unsigned long tPots = 0;        

// ---------------- SERVO ----------------
void moverServo(float graus) {
  int us = US_CENTRO + (int)(graus * US_POR_GRAU);
  us = constrain(us, 1000, 2000);   // nunca sai da faixa segura do MG995
  servo.writeMicroseconds(us);
  comandoEnviado = graus;
}

// ---------------- SENSOR ----------------
void escrever(int reg, int valor) {
  barramento->beginTransmission(ENDERECO_MPU);
  barramento->write(reg);
  barramento->write(valor);
  barramento->endTransmission();
}

bool lerSensor(float &ax, float &az, float &gy) {
  barramento->beginTransmission(ENDERECO_MPU);
  barramento->write(0x3B);                           
  if (barramento->endTransmission(false) != 0) return false;   
  barramento->requestFrom(ENDERECO_MPU, 14);
  if (barramento->available() < 14) {
    while (barramento->available()) barramento->read();
    return false;
  }
  int16_t bruto[7];
  for (int i = 0; i < 7; i++) {
    int alto  = barramento->read();
    int baixo = barramento->read();
    bruto[i] = (int16_t)((alto << 8) | baixo);
  }
  if (bruto[0] == 0 && bruto[1] == 0 && bruto[2] == 0) return false;
  ax = bruto[0] / 16384.0;
  az = bruto[2] / 16384.0;
  gy = bruto[5] / 131.0;
  return true;
}

bool responde(TwoWire &pinos) {
  pinos.begin();
  pinos.setClock(100000);
  pinos.beginTransmission(ENDERECO_MPU);
  pinos.write(0x75);
  return pinos.endTransmission() == 0;
}

bool iniciarSensor() {
  if (responde(Wire)) {
    barramento = &Wire;
    Monitor.println("Sensor encontrado nos pinos SDA/SCL do topo.");
  } else if (responde(Wire2)) {
    barramento = &Wire2;
    Monitor.println("Sensor encontrado nos pinos A4/A5.");
  } else {
    return false;
  }

  escrever(0x6B, 0);                
  escrever(0x1A, 3);                
  escrever(0x1B, 0);                
  escrever(0x1C, 0);                
  delay(100);                       

  Monitor.println("Calibrando: deixe a colher PARADA...");
  float ax, az, gy, soma = 0;
  int boas = 0;
  for (int i = 0; i < 200; i++) {
    if (lerSensor(ax, az, gy)) { soma += gy; boas++; }
    delay(5);
  }
  if (boas < 100) return false;     
  biasGy = soma / boas;

  if (!lerSensor(ax, az, gy)) return false;
  angulo = atan2(-ax, az) * 57.2958;   
  comando = comandoEnviado;         
  tAnterior = micros();
  tUltimaBoa = millis();
  return true;
}

// POTENCIOMETROS 
void lerPotenciometros(bool suave) {
  if (!USAR_POTS) return;
  float peso = suave ? 0.2 : 1.0;

  int leitura = analogRead(PINO_POT_ALVO);            
  if (leitura >= 0) {
    leitura = constrain(leitura, 0, 1023);
    float novo = (leitura / 1023.0 * 2.0 - 1.0) * ALVO_MAX;   
    if (fabs(novo) < 1.5) novo = 0;                   
    alvo = (1 - peso) * alvo + peso * novo;
  }

  leitura = analogRead(PINO_POT_SUAVE);
  if (leitura >= 0) {
    leitura = constrain(leitura, 0, 1023);
    float novo = PASSO_MINIMO + leitura / 1023.0 * (PASSO_MAXIMO - PASSO_MINIMO);
    passo = (1 - peso) * passo + peso * novo;
  }
}

// INICIO 
void setup() {
  servo.attach(PINO_SERVO);
  moverServo(0);                    

  Bridge.begin();                   
  Monitor.begin();
  delay(3000);

  lerPotenciometros(false);
  sensorOk = iniciarSensor();
  if (sensorOk) Monitor.println("Pronto.");
}

// CICLO 
void loop() {
  if (!sensorOk) {
    moverServo(0);
    comando = 0;
    Monitor.println("ERRO: MPU6050 nao respondeu. Colher no centro. Tentando de novo...");
    delay(2000);
    sensorOk = iniciarSensor();
    if (sensorOk) Monitor.println("Pronto.");
    return;
  }

  float ax, az, gy;
  if (!lerSensor(ax, az, gy)) {
    if (millis() - tUltimaBoa > TEMPO_SEM_SENSOR) sensorOk = false;
    tAnterior = micros();
    delay(10);
    return;
  }
  tUltimaBoa = millis();

  unsigned long agora = micros();
  float dt = (agora - tAnterior) / 1000000.0;        
  tAnterior = agora;
  float anguloAcc = atan2(-ax, az) * 57.2958;
  if (dt > 0.2) {
    angulo = anguloAcc;             
  } else {
    angulo = ALFA * (angulo + (gy - biasGy) * dt) + (1 - ALFA) * anguloAcc;
  }

  if (millis() - tPots >= 50) {
    tPots = millis();
    lerPotenciometros(true);
  }

  float desejado = SINAL * (alvo - angulo);
  desejado = constrain(desejado, -LIMITE, LIMITE);
  if (fabs(angulo) > ANG_SEGURANCA) desejado = 0;    

  comando += constrain(desejado - comando, -passo, passo);

  if (fabs(comando - comandoEnviado) >= ZONA_MORTA) {
    moverServo(comando);
  }

  if (millis() - tTela >= 100) {
    tTela = millis();
    Monitor.print("angulo_cabo:");
    Monitor.print(angulo, 1);
    Monitor.print(",compensacao:");
    Monitor.print(comandoEnviado, 1);
    Monitor.print(",alvo:");
    Monitor.println(alvo, 1);
  }
  delay(10);                        
}
