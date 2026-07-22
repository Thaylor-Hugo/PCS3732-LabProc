# Plano de Depuração — Metrônomo RPi3

## Objetivo

Definir como detectar, isolar e corrigir problemas que surgirem durante a
execução isolada dos módulos e durante as etapas do
[Plano de Integração](plano_integracao.md), sem precisar reescrever código
"no escuro".

## Princípio geral: isolar antes de corrigir

Sempre que um sintoma aparecer durante a integração, o primeiro passo é
**voltar para a etapa anterior estável** (ver Plano de Integração) e
reintroduzir apenas o componente suspeito, em vez de tentar corrigir com
os três atuadores rodando ao mesmo tempo. Isso reduz o espaço de busca do
problema de "3 atuadores + timing" para "1 atuador + timing".

## Sintomas conhecidos e como investigar cada um

### 1. Jitter alto (drift > 5ms) ou instável

**Causas prováveis:**
- Corpo do loop demorando mais que o esperado (ex.: chamadas de
  `print()` excessivas, sweep de ângulo do servo grau a grau em vez de
  atribuição direta).
- PWM por software (gpiozero sem `pigpio`) sofrendo jitter do
  escalonador do Linux, conforme "Restrição de Tempo Real" na página 3 do
  PDF de referência (Raspbian padrão não é RTOS).
- Múltiplos atuadores por software PWM competindo por CPU no mesmo
  instante.

**Como depurar:**
- Usar os valores de `drift_time` já impressos por `metronome.py` para
  medir exatamente quanto do 1 segundo está sendo consumido pelo corpo do
  loop (função `beat()`) antes do `sleep`.
- Comentar temporariamente um atuador por vez e observar se o jitter cai —
  aponta qual atuador está custando mais tempo de CPU.
- Se o jitter persistir mesmo com o loop mínimo (Etapa 1 do Plano de
  Integração), considerar instalar o backend `pigpio` (PWM por hardware,
  imune à carga do SO, conforme página 2 do PDF) em vez do PWM por
  software padrão do gpiozero.

### 2. Servo, LED e buzzer não disparam no mesmo instante (dessincronia perceptível)

**Causas prováveis:**
- Chamadas sequenciais em `beat()` com operações bloqueantes entre elas
  (ex.: algum `time.sleep()` extra inserido por engano entre os
  `led.value = 1.0` / `servo.angle = angle` / `buzzer.on()`).
- Atribuição de ângulo ao servo (`AngularServo.angle`) tem custo de
  cálculo de pulso maior que ligar o LED/buzzer — diferença geralmente
  imperceptível, mas deve ser medida, não assumida.

**Como depurar:**
- Adicionar timestamps (`time.time()`) antes e depois de cada linha
  dentro de `beat()` e imprimir a diferença, isolando qual chamada
  individual consome mais tempo.
- Gravar vídeo em câmera lenta do LED + servo + buzzer disparando e
  comparar quadro a quadro.

### 3. Botões não respondem ou disparam múltiplas vezes por clique (desafio)

**Causas prováveis:**
- Bouncing mecânico do botão não filtrado (ver página 8 do PDF —
  "Fenômeno do Ruído Físico").
- `bounce_time` do `gpiozero.Button` insuficiente ou ausente.
- BPM sendo lido e escrito por threads diferentes sem proteção (race
  condition), mesmo que rara.

**Como depurar:**
- Testar `button_up`/`button_down` isoladamente (fora do metrônomo, só
  imprimindo o evento) para confirmar que o próprio clique já está limpo
  antes de acoplar à lógica de BPM.
- Se still houver múltiplos disparos, aumentar `BUTTON_BOUNCE_TIME_S` em
  `config.py` gradualmente (ex.: de 0.2 para 0.3) e testar de novo.
- Confirmar que `_bpm` só é lido/escrito dentro do `_bpm_lock` (já
  implementado em `metronome_desafio.py`) — se o sintoma for uma leitura
  de BPM "no meio" de uma escrita, o lock está faltando em algum ponto.

### 4. Servo vibra ou "range" errado (não bate 0°/180° corretamente)

**Causas prováveis:**
- `min_pulse_width`/`max_pulse_width` não calibrados para o SG90
  específico do kit (variação de fabricação).
- Servo sem alimentação estável (queda de tensão ao acionar simultaneamente
  com outros periféricos).

**Como depurar:**
- Rodar `servo_pwm_test.py` isoladamente e ajustar `SERVO_MIN_PULSE_S` /
  `SERVO_MAX_PULSE_S` em `config.py` em passos pequenos (ex.: 0.05ms) até
  o movimento bater com os ângulos esperados.
- Medir a tensão de alimentação do servo com multímetro durante o
  acionamento simultâneo dos três atuadores, para descartar queda de
  tensão como causa.

## Ferramentas de apoio

- `print()`s de instrumentação já presentes no código (`drift_time`,
  `sleep_time`, eventos de botão).
- Testes isolados (`led_pwm_test.py`, `servo_pwm_test.py`,
  `buzzer_test.py`) como "ambiente controlado" para reproduzir sintomas
  fora do sistema integrado.
- Se disponível em laboratório: analisador lógico ou osciloscópio nos
  pinos de PWM, conforme sugerido na Matriz de Testes do PDF (página 4).

## O que registrar no relatório

Para cada sintoma efetivamente encontrado durante os testes reais: a causa
raiz identificada, o método usado para isolá-la, a correção aplicada e o
resultado da correção (jitter antes/depois, comportamento antes/depois).
Se nenhum sintoma ocorrer em alguma categoria, registrar isso também —
"testado, não reproduzido" é uma evidência válida.
