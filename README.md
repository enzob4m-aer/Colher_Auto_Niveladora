## Colher_Auto_Niveladora
Protótipo de uma colher nivelada quando a mão se inclina, pensada para pessoas com dificuldade motora, feito com: Arduino, MPU6050, Servo MG995 e peças impressas em 3D

## Como funciona:
Um sensor de inclinação (MPU6050) preso no cabo mede o quanto a mão
inclinou. O Arduino calcula o giro contrário e o servo gira a colher,
que continua nivelada.

## O que funciona hoje:
- Estrutura mecânica projetada no Fusion 360 e impressa em 3D
- Controle do servo com movimento suave, comandado por potenciômetro
- Geração do sinal do servo sem biblioteca, para eliminar a tremedeira
  que a biblioteca padrão causava nesta placa

## O que faltou:
O sensor MPU6050 apresentou defeito, identificado com um teste pino a
pino (`ponta_de_prova`). O código de nivelamento (`colher_niveladora`)
está pronto e foi validado em simulação; o teste real será feito com
um sensor novo.
