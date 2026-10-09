// =============================================================
//  COLHER NIVELADORA - CODIGO FINAL (Arduino UNO Q)
//
//  O que faz, 100 vezes por segundo:
//   1) Le o MPU6050 (preso no CABO) e calcula a inclinacao do cabo.
//   2) Calcula quanto o servo precisa girar para a COLHER ficar
//      no angulo desejado (o "alvo"), mesmo com o cabo inclinado.
//   3) Move o servo de forma suave.
//
//  Potenciometros (opcionais - veja USAR_POTS):
//   A2 = alvo: angulo em que a colher deve ficar (-20 a +20 graus; meio = nivelada)
//   A3 = suavidade: quao rapido o servo corrige (um lado = lento/suave, o outro = rapido)
//
//  Protecoes:
//   - Sensor nao responde -> colher volta ao centro e o programa tenta reconectar sozinho.
//   - Cabo inclinado demais (mais que ANG_SEGURANCA) -> para de compensar.
//   - O pulso do servo nunca sai da faixa segura (1000 a 2000).
//
//  AO LIGAR: deixe a colher PARADA, apoiada na mesa, por uns 5 segundos.
// =============================================================
#include <Arduino_RouterBridge.h>
#include <Wire.h>
#include <Servo.h>

// ----------------------- AJUSTES -----------------------
// Estes 3 voce ja acertou no teste 3: COPIE os mesmos valores para ca.
const int   SINAL         = 1;      // se a colher girar para o lado ERRADO, troque para -1
const int   US_CENTRO     = 1500;   // pulso com a colher alinhada ao cabo
const float US_POR_GRAU   = 10.0;   // girou demais? diminua. Girou de menos? aumente

const bool  USAR_POTS     = true;   // false = ignora os potenciometros e usa os valores fixos abaixo
const float ALVO_FIXO     = 0.0;    // usado se USAR_POTS = false (0 = colher nivelada)
const float PASSO_FIXO    = 3.0;    // usado se USAR_POTS = false (graus por ciclo)

const int   PINO_SERVO     = 9;
const int   PINO_POT_ALVO  = A2;
const int   PINO_POT_SUAVE = A3;
const float ALVO_MAX      = 20.0;   // o potenciometro do alvo vai de -20 a +20 graus
const float PASSO_MINIMO  = 1.0;    // potenciometro da suavidade todo para um lado
const float PASSO_MAXIMO  = 5.0;    // potenciometro da suavidade todo para o outro

const float LIMITE        = 45.0;   // maximo que o servo compensa (diminua se a colher bater na peca)
const float ZONA_MORTA    = .0;    // ignora mudancas menores (menor = mais preciso, mas o servo pode zumbir)
const float ANG_SEGURANCA = 70.0;   // cabo mais inclinado que isso: para de compensar
const float ALFA          = 0.98;   // filtro complementar: 98% giroscopio + 2% acelerometro
const unsigned long TEMPO_SEM_SENSOR = 300;   // milissegundos sem leitura boa para considerar o sensor perdido
// -------------------------------------------------------

const uint8_t ENDERECO_MPU = 0x68;

Servo servo;
TwoWire *barramento = &Wire;        // guarda EM QUAL conjunto de pinos o sensor esta (topo ou A4/A5)

float biasGy = 0;                   // erro de fabrica do giroscopio (medido na calibracao)
float angulo = 0;                   // inclinacao do cabo, em graus
float alvo = ALVO_FIXO;             // angulo desejado da colher
float passo = PASSO_FIXO;           // maximo de graus que o comando muda por ciclo
float comando = 0;                  // compensacao calculada (graus, relativa ao centro)
float comandoEnviado = 0;           // ultima compensacao mandada ao servo
bool sensorOk = false;
unsigned long tAnterior = 0;        // hora da ultima leitura boa (microssegundos)
unsigned long tUltimaBoa = 0;       // hora da ultima leitura boa (milissegundos)
unsigned long tTela = 0;            // hora da ultima linha na tela (milissegundos)
unsigned long tPots = 0;            // hora da ultima leitura dos potenciometros

// ---------------- SERVO ----------------
// Converte graus de compensacao em pulso e manda ao servo
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

// Le as medidas. Devolve false se a leitura falhar.
// ax e az em g, gy em graus/s
bool lerSensor(float &ax, float &az, float &gy) {
  barramento->beginTransmission(ENDERECO_MPU);
  barramento->write(0x3B);                           // primeiro registrador de dados
  if (barramento->endTransmission(false) != 0) return false;   // sensor nao atendeu
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
  // Acelerometro todo em zero = o sensor reiniciou e voltou a "dormir": conta como falha
  if (bruto[0] == 0 && bruto[1] == 0 && bruto[2] == 0) return false;
  ax = bruto[0] / 16384.0;
  az = bruto[2] / 16384.0;
  gy = bruto[5] / 131.0;
  return true;
}

// Pergunta se ha um MPU6050 nesse conjunto de pinos
bool responde(TwoWire &pinos) {
  pinos.begin();
  pinos.setClock(100000);
  pinos.beginTransmission(ENDERECO_MPU);
  pinos.write(0x75);
  return pinos.endTransmission() == 0;
}

// Procura o sensor, configura e calibra. Devolve true se deu tudo certo.
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

  escrever(0x6B, 0);                // acorda o sensor
  escrever(0x1A, 3);                // filtro interno (~44 Hz)
  escrever(0x1B, 0);                // giroscopio +-250 graus/s
  escrever(0x1C, 0);                // acelerometro +-2 g
  delay(100);                       // tempo para o sensor estabilizar

  Monitor.println("Calibrando: deixe a colher PARADA...");
  float ax, az, gy, soma = 0;
  int boas = 0;
  for (int i = 0; i < 200; i++) {
    if (lerSensor(ax, az, gy)) { soma += gy; boas++; }
    delay(5);
  }
  if (boas < 100) return false;     // mais da metade falhou: fio ruim
  biasGy = soma / boas;

  if (!lerSensor(ax, az, gy)) return false;
  angulo = atan2(-ax, az) * 57.2958;   // angulo inicial so pelo acelerometro
  comando = comandoEnviado;         // continua de onde o servo esta
  tAnterior = micros();
  tUltimaBoa = millis();
  return true;
}

// ---------------- POTENCIOMETROS ----------------
// suave = true: mistura com o valor anterior (tira o tremido da leitura)
void lerPotenciometros(bool suave) {
  if (!USAR_POTS) return;
  float peso = suave ? 0.2 : 1.0;

  int leitura = analogRead(PINO_POT_ALVO);            // 0 a 1023
  if (leitura >= 0) {
    leitura = constrain(leitura, 0, 1023);
    float novo = (leitura / 1023.0 * 2.0 - 1.0) * ALVO_MAX;   // -20 a +20
    if (fabs(novo) < 1.5) novo = 0;                   // perto do meio = exatamente nivelada
    alvo = (1 - peso) * alvo + peso * novo;
  }

  leitura = analogRead(PINO_POT_SUAVE);
  if (leitura >= 0) {
    leitura = constrain(leitura, 0, 1023);
    float novo = PASSO_MINIMO + leitura / 1023.0 * (PASSO_MAXIMO - PASSO_MINIMO);
    passo = (1 - peso) * passo + peso * novo;
  }
}

// ---------------- INICIO ----------------
void setup() {
  servo.attach(PINO_SERVO);
  moverServo(0);                    // centro antes de qualquer coisa

  Bridge.begin();                   // SEMPRE antes do Monitor
  Monitor.begin();
  delay(3000);

  lerPotenciometros(false);
  sensorOk = iniciarSensor();
  if (sensorOk) Monitor.println("Pronto.");
}

// ---------------- CICLO ----------------
void loop() {
  // Sem sensor: colher no centro, avisa e tenta reconectar
  if (!sensorOk) {
    moverServo(0);
    comando = 0;
    Monitor.println("ERRO: MPU6050 nao respondeu. Colher no centro. Tentando de novo...");
    delay(2000);
    sensorOk = iniciarSensor();
    if (sensorOk) Monitor.println("Pronto.");
    return;
  }

  // 1) Ler o sensor. Falha isolada: mantem a posicao. Falhando ha muito tempo: sensor perdido.
  float ax, az, gy;
  if (!lerSensor(ax, az, gy)) {
    if (millis() - tUltimaBoa > TEMPO_SEM_SENSOR) sensorOk = false;
    tAnterior = micros();
    delay(10);
    return;
  }
  tUltimaBoa = millis();

  // 2) Calcular a inclinacao do cabo
  unsigned long agora = micros();
  float dt = (agora - tAnterior) / 1000000.0;        // segundos desde a ultima leitura
  tAnterior = agora;
  float anguloAcc = atan2(-ax, az) * 57.2958;
  if (dt > 0.2) {
    angulo = anguloAcc;             // ficou muito tempo sem ler: recomeca pelo acelerometro
  } else {
    angulo = ALFA * (angulo + (gy - biasGy) * dt) + (1 - ALFA) * anguloAcc;
  }

  // 3) Ler os potenciometros (20 vezes por segundo e suficiente)
  if (millis() - tPots >= 50) {
    tPots = millis();
    lerPotenciometros(true);
  }

  // 4) Calcular quanto compensar: a colher deve ficar no alvo, o cabo esta em "angulo"
  float desejado = SINAL * (alvo - angulo);
  desejado = constrain(desejado, -LIMITE, LIMITE);
  if (fabs(angulo) > ANG_SEGURANCA) desejado = 0;    // colher caiu ou virou: para

  // 5) Suavizar: o comando muda no maximo "passo" graus por ciclo
  comando += constrain(desejado - comando, -passo, passo);

  // 6) So move se a mudanca for maior que a zona morta
  if (fabs(comando - comandoEnviado) >= ZONA_MORTA) {
    moverServo(comando);
  }

  // 7) Dados 10x por segundo (servem no Monitor Serial e no Plotter Serial)
  if (millis() - tTela >= 100) {
    tTela = millis();
    Monitor.print("angulo_cabo:");
    Monitor.print(angulo, 1);
    Monitor.print(",compensacao:");
    Monitor.print(comandoEnviado, 1);
    Monitor.print(",alvo:");
    Monitor.println(alvo, 1);
  }
  delay(10);                        // ~100 ciclos por segundo
}
