// =============================================================
//  DEMONSTRACAO SEM SENSOR (Arduino UNO Q) - versao "anda e para"
//
//  O potenciometro de A2 escolhe o angulo da colher (-45 a +45 graus).
//  A colher vai ate la em movimento continuo e PARA (servo desligado).
//
//  Por que "anda e para": quando o servo se move, ele suja a leitura do
//  potenciometro (os dois dividem o mesmo GND). O programa lia esse ruido
//  como "botao girou" e mandava mover de novo -> tremedeira sem fim.
//  Agora o botao so e lido com o servo DESLIGADO e quieto.
//
//  MODO_AUTOMATICO = true: ignora o botao e passeia sozinho por alguns
//  angulos (garantia para gravar o video).
// =============================================================
#include <Arduino_RouterBridge.h>

// ----------------------- AJUSTES -----------------------
const bool  MODO_AUTOMATICO     = false;  // true = movimenta sozinho, sem potenciometro
const int   PINO_SERVO          = 9;
const int   PINO_POT_ANGULO     = A2;
const float PASSO               = 1.0;    // graus por ciclo de 20 ms (1.0 = 50 graus por segundo)
const int   SINAL               = 1;      // 1 ou -1: inverte o lado
const int   US_CENTRO           = 1500;   // pulso com a colher alinhada ao cabo
const float US_POR_GRAU         = 10.0;
const float LIMITE              = 45.0;   // diminua se a colher bater na peca
const float MUDANCA_MINIMA      = 4.0;    // o botao precisa mudar isso (graus) para a colher reagir
const float TOLERANCIA          = 1.5;    // leituras dentro disso contam como "botao parado"
const int   CICLOS_BOTAO_PARADO = 8;      // botao parado por 8 ciclos (0,16 s) = pedido aceito
const int   CICLOS_SEGURANDO    = 10;     // depois de chegar, segura 0,2 s e desliga o servo
const int   CICLOS_DESCANSO     = 15;     // depois de desligar, espera 0,3 s antes de ler o botao
const unsigned long CICLO_US    = 20000;  // 20 ms = 50 pulsos por segundo
// -------------------------------------------------------

// Angulos do modo automatico, em graus (passa por eles em ordem e repete)
const float ROTEIRO[6] = {30, 0, -30, 0, 15, -15};
const int   CICLOS_PAUSA_AUTOMATICO = 75;    // 1,5 s parado em cada angulo

float pedido = 0;                   // destino atual da colher
float comando = 0;                  // angulo que esta indo para o servo
float candidato = 0;                // leitura do botao que esta sendo conferida
int ciclosCandidato = 0;            // ha quantos ciclos o botao esta parado nesse valor
int ciclosChegou = 0;               // ha quantos ciclos a colher chegou ao destino
int etapa = 0;                      // posicao no roteiro automatico
int usAtual = US_CENTRO;
unsigned long tCiclo = 0;
unsigned long tTela = 0;

// Manda UM pulso ao servo
void pulso() {
  digitalWrite(PINO_SERVO, HIGH);
  delayMicroseconds(usAtual);
  digitalWrite(PINO_SERVO, LOW);
}

// Le o botao e devolve o angulo correspondente (-LIMITE a +LIMITE)
float lerBotao() {
  long soma = 0;
  int boas = 0;
  for (int i = 0; i < 8; i++) {                 // media de 8 leituras
    int leitura = analogRead(PINO_POT_ANGULO);
    if (leitura >= 0) { soma += constrain(leitura, 0, 1023); boas++; }
  }
  if (boas == 0) return pedido;                 // leitura falhou: mantem o destino
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
    // 1) EM MOVIMENTO: anda um passo e manda o pulso. Nao le o botao.
    comando += constrain(pedido - comando, -PASSO, PASSO);
    int us = US_CENTRO + (int)(SINAL * comando * US_POR_GRAU);
    usAtual = constrain(us, 1000, 2000);
    pulso();
    ciclosChegou = 0;
    ciclosCandidato = 0;
  } else {
    // 2) CHEGOU: segura um instante, depois para de mandar pulsos (servo desliga)
    if (ciclosChegou < CICLOS_SEGURANDO + CICLOS_DESCANSO + CICLOS_PAUSA_AUTOMATICO) ciclosChegou++;
    if (ciclosChegou <= CICLOS_SEGURANDO) {
      pulso();
    } else if (ciclosChegou > CICLOS_SEGURANDO + CICLOS_DESCANSO) {
      // 3) PARADA E QUIETA: so agora escolhe o proximo destino
      if (MODO_AUTOMATICO) {
        if (ciclosChegou >= CICLOS_SEGURANDO + CICLOS_DESCANSO + CICLOS_PAUSA_AUTOMATICO) {
          pedido = constrain(ROTEIRO[etapa], -LIMITE, LIMITE);
          etapa = (etapa + 1) % 6;
        }
      } else {
        float leitura = lerBotao();
        if (fabs(leitura - candidato) <= TOLERANCIA) {
          ciclosCandidato++;                    // botao continua no mesmo lugar
        } else {
          candidato = leitura;                  // botao mexeu: recomeca a conferencia
          ciclosCandidato = 0;
        }
        if (ciclosCandidato >= CICLOS_BOTAO_PARADO && fabs(candidato - pedido) >= MUDANCA_MINIMA) {
          pedido = candidato;                   // pedido aceito: no proximo ciclo comeca a mover
        }
      }
    }
  }

  // 4) Dados 5x por segundo (Monitor Serial ou Plotter Serial)
  if (millis() - tTela >= 200) {
    tTela = millis();
    Monitor.print("pedido:");
    Monitor.print(pedido, 1);
    Monitor.print(",colher:");
    Monitor.println(comando, 1);
  }

  // 5) Completar os 20 ms do ciclo
  while (micros() - tCiclo < CICLO_US) {
    delay(1);
  }
  tCiclo = micros();
}
