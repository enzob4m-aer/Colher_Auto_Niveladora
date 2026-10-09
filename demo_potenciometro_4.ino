//  DEMONSTRACAO SEM SENSOR (Arduino UNO Q)
//
//  O potenciometro de A2 escolhe o angulo da colher (-45 a +45 graus).
//  A colher vai ate la em movimento continuo e PARA (servo desligado).

#include <Arduino_RouterBridge.h>

// AJUSTES 
const bool  MODO_AUTOMATICO     = false;  
const int   PINO_SERVO          = 9;
const int   PINO_POT_ANGULO     = A2;
const float PASSO               = 1.0;    
const int   SINAL               = 1;      
const int   US_CENTRO           = 1500;   
const float US_POR_GRAU         = 10.0;
const float LIMITE              = 45.0;   
const float MUDANCA_MINIMA      = 4.0;    
const float TOLERANCIA          = 1.5;    
const int   CICLOS_BOTAO_PARADO = 8;      
const int   CICLOS_SEGURANDO    = 10;     
const int   CICLOS_DESCANSO     = 15;     
const unsigned long CICLO_US    = 20000;  

const float ROTEIRO[6] = {30, 0, -30, 0, 15, -15};
const int   CICLOS_PAUSA_AUTOMATICO = 75;    

float pedido = 0;               
float comando = 0;               
float candidato = 0;              
int ciclosCandidato = 0;       
int ciclosChegou = 0;           
int etapa = 0;            
int usAtual = US_CENTRO;
unsigned long tCiclo = 0;
unsigned long tTela = 0;

void pulso() {
  digitalWrite(PINO_SERVO, HIGH);
  delayMicroseconds(usAtual);
  digitalWrite(PINO_SERVO, LOW);
}

float lerBotao() {
  long soma = 0;
  int boas = 0;
  for (int i = 0; i < 8; i++) {
    int leitura = analogRead(PINO_POT_ANGULO);
    if (leitura >= 0) { soma += constrain(leitura, 0, 1023); boas++; }
  }
  if (boas == 0) return pedido;                
  float media = (float)soma / boas;
  return (media / 1023.0 * 2.0 - 1.0) * LIMITE;
}

void setup() {
  pinMode(PINO_SERVO, OUTPUT);
  digitalWrite(PINO_SERVO, LOW);
  Bridge.begin();
  Monitor.begin();
  delay(3000);
  tCiclo = micros();
}

void loop() {
  bool movendo = (comando != pedido);

  if (movendo) {
    
    comando += constrain(pedido - comando, -PASSO, PASSO);
    int us = US_CENTRO + (int)(SINAL * comando * US_POR_GRAU);
    usAtual = constrain(us, 1000, 2000);
    pulso();
    ciclosChegou = 0;
    ciclosCandidato = 0;
  } else {
    
    if (ciclosChegou < CICLOS_SEGURANDO + CICLOS_DESCANSO + CICLOS_PAUSA_AUTOMATICO) ciclosChegou++;
    if (ciclosChegou <= CICLOS_SEGURANDO) {
      pulso();
    } else if (ciclosChegou > CICLOS_SEGURANDO + CICLOS_DESCANSO) {

            if (MODO_AUTOMATICO) {
        if (ciclosChegou >= CICLOS_SEGURANDO + CICLOS_DESCANSO + CICLOS_PAUSA_AUTOMATICO) {
          pedido = constrain(ROTEIRO[etapa], -LIMITE, LIMITE);
          etapa = (etapa + 1) % 6;
        }
      } else {
        float leitura = lerBotao();
        if (fabs(leitura - candidato) <= TOLERANCIA) {
          ciclosCandidato++;                   
        } else {
          candidato = leitura;                 
          ciclosCandidato = 0;
        }
        if (ciclosCandidato >= CICLOS_BOTAO_PARADO && fabs(candidato - pedido) >= MUDANCA_MINIMA) {
          pedido = candidato;                 
        }
      }
    }
  }

  if (millis() - tTela >= 200) {
    tTela = millis();
    Monitor.print("pedido:");
    Monitor.print(pedido, 1);
    Monitor.print(",colher:");
    Monitor.println(comando, 1);
  }

  while (micros() - tCiclo < CICLO_US) {
    delay(1);
  }
  tCiclo = micros();
}
