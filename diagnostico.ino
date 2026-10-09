// =============================================================
//  DIAGNOSTICO GERAL (Arduino UNO Q) - sensor MPU6050 + servo
//
//  PARTE 1 - SERVO (roda UMA vez, logo depois de ligar/enviar):
//    fase 1: centro, pulso gerado "na mao"            (8 s)
//    pausa : sem sinal                                 (3 s)
//    fase 2: centro, biblioteca Servo, placa quieta    (8 s)
//    fase 3: centro, biblioteca Servo, placa ocupada   (8 s)
//    fase 4: desligado
//    -> Anote em QUAIS fases o servo treme.
//
//  PARTE 2 - SENSOR (repete a cada 5 s, para sempre):
//    Testa cada fio (SDA e SCL) separadamente, sem depender da
//    biblioteca Wire, nos pinos do topo e em A4/A5, e diz o que achou.
//
//  Ligacoes: as mesmas de sempre. Nao precisa mudar nada.
// =============================================================
#include <Arduino_RouterBridge.h>
#include <Wire.h>
#include <Servo.h>

const int PINO_SERVO   = 9;
const int DADOS_TOPO   = 20;        // pino "SDA" do topo da placa
const int RELOGIO_TOPO = 21;        // pino "SCL" do topo da placa
const int DADOS_A4     = A4;        // A4 tambem serve de SDA
const int RELOGIO_A5   = A5;        // A5 tambem serve de SCL

Servo *servoDaBiblioteca = nullptr; // so e criado na fase 2 (de proposito)

// ---------------------------------------------------------------
//  SENSOR: teste fio a fio
// ---------------------------------------------------------------
// "Soltar" a linha: ela sobe sozinha para 3,3 V. "Baixar": forca 0 V.
// (E assim que o I2C funciona: ninguem empurra para cima, so puxa para baixo.)
void soltar(int pino) { pinMode(pino, INPUT_PULLUP); }
void baixar(int pino) { pinMode(pino, OUTPUT); }      // OUTPUT ja comeca em 0 V

// Descobre o estado eletrico de uma linha:
//   0 = presa em 0 V
//   1 = livre (nada ligado nela)
//   2 = ligada a algo que puxa para 3,3 V (o sensor tem um resistor que faz isso)
int estadoDaLinha(int pino) {
  pinMode(pino, INPUT_PULLUP);
  delay(5);
  bool altaPuxandoParaCima = (digitalRead(pino) == HIGH);
  pinMode(pino, INPUT_PULLDOWN);
  delay(5);
  bool altaPuxandoParaBaixo = (digitalRead(pino) == HIGH);
  pinMode(pino, INPUT_PULLUP);
  delay(5);
  if (!altaPuxandoParaCima) return 0;
  if (!altaPuxandoParaBaixo) return 1;
  return 2;
}

void mostrarEstado(const char* nome, int estado) {
  Monitor.print("   fio ");
  Monitor.print(nome);
  if (estado == 0) Monitor.println(": PRESO EM 0 V (curto com o GND, ou sensor sem energia)");
  if (estado == 1) Monitor.println(": SOLTO - nao chega ao sensor (fio, jumper ou solda)");
  if (estado == 2) Monitor.println(": ok, chega ao sensor");
}

// Chama um endereco "na mao" e devolve true se alguem atendeu
bool perguntar(int pinoDados, int pinoRelogio, uint8_t endereco) {
  const int T = 50;                                   // microssegundos por etapa (bem devagar)
  soltar(pinoDados);
  soltar(pinoRelogio);
  delayMicroseconds(200);
  if (digitalRead(pinoDados) == LOW || digitalRead(pinoRelogio) == LOW) return false;

  baixar(pinoDados);   delayMicroseconds(T);          // sinal de INICIO
  baixar(pinoRelogio); delayMicroseconds(T);

  uint8_t dado = endereco << 1;                       // 7 bits de endereco + 1 bit "escrever"
  for (int i = 7; i >= 0; i--) {
    if (dado & (1 << i)) soltar(pinoDados); else baixar(pinoDados);
    delayMicroseconds(T);
    soltar(pinoRelogio); delayMicroseconds(T);        // o sensor le o bit aqui
    baixar(pinoRelogio); delayMicroseconds(T);
  }

  soltar(pinoDados);   delayMicroseconds(T);          // 9o pulso: a vez do sensor
  soltar(pinoRelogio); delayMicroseconds(T);
  bool atendeu = (digitalRead(pinoDados) == LOW);     // sensor puxou para 0 V = "estou aqui"
  baixar(pinoRelogio); delayMicroseconds(T);

  baixar(pinoDados);   delayMicroseconds(T);          // sinal de FIM
  soltar(pinoRelogio); delayMicroseconds(T);
  soltar(pinoDados);   delayMicroseconds(T);
  return atendeu;
}

bool perguntarOsDois(int pinoDados, int pinoRelogio) {
  return perguntar(pinoDados, pinoRelogio, 0x68) || perguntar(pinoDados, pinoRelogio, 0x69);
}

// Mesma pergunta, mas pela biblioteca Wire (como os outros programas fazem)
void perguntarPelaWire(TwoWire &barramento) {
  barramento.begin();                                 // devolve os pinos para o I2C
  barramento.setClock(100000);
  unsigned long inicio = millis();
  barramento.beginTransmission(0x68);
  barramento.write(0x75);
  uint8_t resultado = barramento.endTransmission();
  unsigned long tempo = millis() - inicio;
  Monitor.print("   pela biblioteca Wire (0x68): ");
  if (resultado == 0)      Monitor.print("RESPONDEU");
  else if (tempo >= 300)   Monitor.print("TRAVOU");
  else                     Monitor.print("nao respondeu");
  Monitor.print(" (");
  Monitor.print(tempo);
  Monitor.println(" ms)");
}

void testarPinos(const char* nome, const char* nomeDados, const char* nomeRelogio,
                 int pinoDados, int pinoRelogio, TwoWire &barramento) {
  Monitor.println(nome);
  int estadoDados   = estadoDaLinha(pinoDados);
  int estadoRelogio = estadoDaLinha(pinoRelogio);
  mostrarEstado(nomeDados, estadoDados);
  mostrarEstado(nomeRelogio, estadoRelogio);

  Monitor.print("   RESULTADO: ");
  if (estadoDados == 0 || estadoRelogio == 0) {
    Monitor.println("ha um fio preso em 0 V. Tire esse fio do Arduino e veja se muda para SOLTO.");
  } else if (estadoDados == 1 && estadoRelogio == 1) {
    Monitor.println("nada ligado nestes pinos.");
  } else if (perguntarOsDois(pinoDados, pinoRelogio)) {
    Monitor.println("SENSOR RESPONDEU! Ligacao correta.");
  } else if (perguntarOsDois(pinoRelogio, pinoDados)) {
    Monitor.println("SENSOR RESPONDEU COM OS FIOS TROCADOS: inverta SDA e SCL.");
  } else if (estadoDados == 1 || estadoRelogio == 1) {
    Monitor.println("so um dos dois fios chega ao sensor. Conserte o que esta SOLTO.");
  } else {
    Monitor.println("os dois fios chegam, mas o sensor NAO responde (pino errado no sensor, GND ruim ou sensor com defeito).");
  }
  perguntarPelaWire(barramento);
}

// ---------------------------------------------------------------
//  SERVO: quatro fases
// ---------------------------------------------------------------
// Gera os pulsos do servo sem biblioteca: 1 pulso a cada ~20 ms
void pulsosNaMao(int us, unsigned long duracaoMs) {
  pinMode(PINO_SERVO, OUTPUT);
  unsigned long inicio = millis();
  while (millis() - inicio < duracaoMs) {
    digitalWrite(PINO_SERVO, HIGH);
    delayMicroseconds(us);
    digitalWrite(PINO_SERVO, LOW);
    delay(18);
  }
}

void testarServo() {
  Monitor.println("===== PARTE 1: SERVO (olhe para o servo) =====");

  Monitor.println("FASE 1 de 4: centro, pulso gerado na mao (8 s). Treme?");
  delay(200);
  pulsosNaMao(1500, 8000);

  Monitor.println("pausa de 3 s (sem sinal: o servo fica mole)");
  delay(3000);

  Monitor.println("FASE 2 de 4: centro, biblioteca Servo, placa quieta (8 s). Treme? Mudou de posicao?");
  delay(200);
  servoDaBiblioteca = new Servo();
  servoDaBiblioteca->attach(PINO_SERVO);
  servoDaBiblioteca->writeMicroseconds(1500);
  delay(8000);

  Monitor.println("FASE 3 de 4: centro, biblioteca Servo, placa ocupada (8 s). Treme mais?");
  unsigned long inicio = millis();
  unsigned long tPonto = 0;
  while (millis() - inicio < 8000) {
    Wire1.beginTransmission(0x68);                    // conversa I2C no conector vazio,
    Wire1.write(0x75);                                // so para dar trabalho a placa
    Wire1.endTransmission();
    analogRead(A0);
    if (millis() - tPonto >= 100) {
      tPonto = millis();
      Monitor.print(".");
    }
    delay(10);
  }
  Monitor.println("");

  Monitor.println("FASE 4 de 4: servo desligado. Deve ficar mole e em silencio.");
  servoDaBiblioteca->detach();
  digitalWrite(PINO_SERVO, LOW);
  Monitor.println("===== FIM DA PARTE 1 =====");
  Monitor.println("");
}

// ---------------------------------------------------------------
void setup() {
  pinMode(PINO_SERVO, OUTPUT);
  digitalWrite(PINO_SERVO, LOW);
  Bridge.begin();
  Monitor.begin();
  delay(5000);                                        // tempo para abrir o Monitor Serial
  Wire1.begin();
  testarServo();
}

void loop() {
  Monitor.println("===== PARTE 2: SENSOR =====");
  testarPinos("PINOS DO TOPO (SDA e SCL, perto do AREF):", "SDA", "SCL",
              DADOS_TOPO, RELOGIO_TOPO, Wire);
  testarPinos("PINOS A4 e A5:", "A4 (SDA)", "A5 (SCL)",
              DADOS_A4, RELOGIO_A5, Wire2);
  Monitor.println("");
  delay(5000);
}
