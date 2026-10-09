// =============================================================
//  PONTA DE PROVA (Arduino UNO Q)
//
//  Transforma o pino A4 num "testador": um fio sai do A4 e a outra
//  ponta voce encosta no ponto que quer medir. A tela mostra, 2 vezes
//  por segundo, o que ha naquele ponto:
//     3,3 V  = ligado ao positivo 
//     0 V    = ligado ao GND 
//     SOLTO  = nao esta encostando em nada
//
//  CUIDADO: nunca encoste a ponta no trilho de 5V da fonte
// =============================================================
#include <Arduino_RouterBridge.h>

const int PINO_PROVA = A4;

// 0 = 0 V | 1 = solto | 2 = 3,3 V
int medir() {
  pinMode(PINO_PROVA, INPUT_PULLUP);      // puxa de leve para cima e olha
  delay(5);
  bool altoPuxandoParaCima = (digitalRead(PINO_PROVA) == HIGH);
  pinMode(PINO_PROVA, INPUT_PULLDOWN);    // puxa de leve para baixo e olha
  delay(5);
  bool altoPuxandoParaBaixo = (digitalRead(PINO_PROVA) == HIGH);
  if (!altoPuxandoParaCima) return 0;     // nem puxando para cima subiu: esta em 0 V
  if (!altoPuxandoParaBaixo) return 1;    // vai para onde eu puxo: esta solto
  return 2;                               // nem puxando para baixo desceu: esta em 3,3 V
}

void setup() {
  Bridge.begin();
  Monitor.begin();
  delay(3000);
  Monitor.println("Ponta de prova no pino A4. Encoste a outra ponta do fio no ponto a medir.");
}

void loop() {
  int resultado = medir();
  if (resultado == 0) Monitor.println("0 V");
  if (resultado == 1) Monitor.println("SOLTO");
  if (resultado == 2) Monitor.println("3,3 V");
  delay(500);
}
