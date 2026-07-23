# Plano de Integração — Fechadura Eletrônica RPi3

## Objetivo

Integrar os quatro módulos desenvolvidos e validados isoladamente
(`keypad_test.py`, `lcd_test.py`, `sensor_test.py`, `buzzer_test.py`) em um
único loop de estados não-bloqueante (`electronic_lock.py`), sem regressão
do comportamento observado em cada teste isolado, seguindo os "degraus" de
integração propostos no PDF de referência (página "O Desafio de
Integração").

## Pré-condições

Cada módulo isolado deve ter sido executado no hardware real com resultado
registrado antes de iniciar a integração ("Regra de Ouro: nunca integre um
componente que não passou em seu próprio teste unitário"):

| Módulo  | Script            | Critério de aceite                                             |
|---------|-------------------|------------------------------------------------------------------|
| Sensor  | `sensor_test.py`  | Transição TRANCADA/ABERTA reflete corretamente a distância medida |
| Teclado | `keypad_test.py`  | Cada pressionamento gera exatamente 1 evento (sem bouncing)      |
| LCD     | `lcd_test.py`     | `i2cdetect -y 1` encontra o display; "Hello World" aparece nas 2 linhas |
| Buzzer  | `buzzer_test.py`  | Bipe curto e bipe longo audíveis e com duração consistente       |

## Estratégia de integração: incremental, não big-bang

### Degrau 1 — Core + Sensor

Loop principal com apenas a leitura contínua do sensor ultrassônico
(`_poll_sensor`), sem teclado/LCD/buzzer. Validar que o estado físico
(TRANCADA/ABERTA) é lido de forma estável a cada `SENSOR_POLL_INTERVAL_S`
sem travar o loop.

**Critério de avanço:** leitura do sensor estável por >= 30 ciclos, sem
bloqueio perceptível do processo.

### Degrau 2 — + Teclado matricial

Acoplar `Keypad.scan()` ao loop principal, atualizando o buffer de senha em
memória. Validar que digitar e apagar (`*`) funciona e que a leitura do
sensor (Degrau 1) continua estável com o teclado sendo varrido.

**Critério de avanço:** entrada de senha funcional sem degradar a resposta
do sensor.

### Degrau 3 — + Display LCD

Acoplar `_render()` para exibir o buffer mascarado (`*`) e o status
(Trancada/Aberto) em tempo real. Validar RF2 (atualização do LCD em
< 200ms após a tecla `#` ser pressionada).

**Critério de avanço:** o LCD reflete cada tecla digitada e cada transição
de estado sem atraso perceptível.

### Degrau 4 — + Buzzer

Acoplar `NonBlockingBuzzer` para os bipes de sucesso/falha. Como o PDF
alerta especificamente contra `sleep()` bloqueante nesta etapa ("congela a
varredura do teclado ou ignora a abertura do sensor"), o critério de avanço
verifica exatamente esse risco.

**Critério de avanço:** o teclado continua responsivo e o sensor continua
sendo lido *enquanto* o buzzer soa (bipe de 0.6s no caso de falha).

## Ordem de execução resumida

```
1. Loop com sensor apenas
2. + Teclado (entrada de senha em memória)
3. + LCD (feedback visual em tempo real)
4. + Buzzer (feedback sonoro não-bloqueante)
5. Versão desafio: hash de senha, verificação de integridade do sensor,
   log encadeado por hash, cooldown com backoff exponencial
```

## Ferramentas de verificação em cada etapa

- `print()`/logs no console em cada transição de estado (evidência do
  fluxo Idle -> Evento de Entrada -> Processamento -> Sucesso/Falha).
- Observação direta do LCD e do buzzer no instante de cada evento.
- `i2cdetect -y 1` e `dmesg | tail` para confirmar reconhecimento do
  barramento I2C pelo SO antes de depurar a lógica do LCD.

## O que registrar no relatório

Para cada degrau: se o critério de avanço foi atingido na primeira
tentativa, qual componente precisou de ajuste de pinagem/tempo, e qualquer
efeito colateral observado ao somar o próximo componente (ex.: teclado
"travando" ao integrar o buzzer, se `sleep()` bloqueante tiver sido usado
por engano).
