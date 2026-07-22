# Plano de Integração — Metrônomo RPi3

## Objetivo

Integrar os três módulos de atuação desenvolvidos e validados isoladamente
(`led_pwm_test.py`, `servo_pwm_test.py`, `buzzer_test.py`) em um único loop
de temporização de 1 segundo (`metronome.py`), sem regressão do
comportamento observado em cada teste isolado.

## Pré-condições

Antes de iniciar a integração, cada módulo isolado deve ter sido executado
no hardware real e ter os resultados registrados (ver "Atividades 1–3" do
enunciado):

| Módulo   | Script              | Critério de aceite                                            |
|----------|---------------------|-----------------------------------------------------------------|
| LED      | `led_pwm_test.py`   | Rampa de brilho visível e suave em todas as frequências testadas |
| Servo    | `servo_pwm_test.py` | Movimento mecânico correto para 0°, 90° e 180°, sem vibração no repouso |
| Buzzer   | `buzzer_test.py`    | Beep audível e de duração consistente a cada acionamento        |

Se qualquer módulo isolado falhar, a integração não deve começar — o
problema deve ser resolvido no módulo isolado primeiro (isola a causa raiz
antes de somar variáveis).

## Estratégia de integração: incremental, não big-bang

Em vez de juntar os três atuadores de uma vez, a integração é feita em
etapas, cada uma testada no hardware antes de avançar para a próxima. Isso
segue diretamente a arquitetura de software da página 6 do PDF de
referência, que separa claramente "thread principal (loop do metrônomo)"
de "hardware PWM (kernel)".

### Etapa 1 — Esqueleto de temporização sem atuadores

Implementar apenas o loop de 1Hz com o cálculo de `drift_time` e
`time.sleep(1.0 - drift_time)`, sem acionar nenhum atuador — apenas um
`print()` marcando cada "tick". Validar que o intervalo entre ticks fica
estável em ~1000ms antes de acoplar qualquer hardware.

**Critério de avanço:** jitter medido entre ticks consecutivos < 5ms
(RF01), consistente em pelo menos 30 ciclos seguidos.

### Etapa 2 — Acoplar um atuador por vez

1. Acoplar somente o **LED** ao tick. Validar que o pulso visual continua
   sincronizado a cada 1s e que o jitter do loop não piorou.
2. Acoplar o **buzzer** ao tick, com o LED já presente. Validar que o beep
   soa junto com o pulso do LED, sem atraso perceptível entre os dois.
3. Acoplar o **servo** ao tick, com LED e buzzer já presentes. Validar que
   o movimento do servo ocorre no mesmo instante do beep/LED.

Acoplar um atuador de cada vez isola qual componente introduz atraso ou
quebra a temporização, caso algo dê errado — se o jitter piorar ao entrar
na etapa 3, por exemplo, o suspeito principal é o servo (PWM por software
mais custoso), não o conjunto todo.

**Critério de avanço:** a cada etapa, jitter continua < 5ms e o atuador
recém-acoplado dispara visivelmente/audivelmente no instante esperado.

### Etapa 3 — Integração completa e alternância do servo

Com os três atuadores estáveis no mesmo tick, adicionar a lógica de
alternância do ângulo do servo entre 0° e 180° a cada batida (efeito
"tic-tac"), validando que a troca de estado não introduz atraso adicional
no cálculo do `drift_time`.

### Etapa 4 (desafio) — Acoplar controle de BPM via botões

Somente após a Etapa 3 estar estável, integrar `metronome_desafio.py`:
thread secundária de escuta dos botões (`Button.when_pressed` do
gpiozero) atualizando a variável global de BPM sob lock, sem que a thread
principal do metrônomo jamais bloqueie esperando por ela — conforme o
diagrama de sequência da página 6 do PDF ("Decisão Arquitetural: a
separação em múltiplas threads... isolando o loop crítico de 1Hz").

**Critério de avanço:** pressionar os botões durante a operação altera o
BPM sem interromper ou atrasar a batida em andamento.

## Ordem de execução resumida

```
1. Loop de timing puro (sem hardware)
2. + LED
3. + Buzzer
4. + Servo
5. + Alternância de ângulo do servo
6. + Botões físicos (thread secundária) [desafio]
```

## Ferramentas de verificação em cada etapa

- `print()` de `drift_time`/`sleep_time` já presentes em `metronome.py`
  (evidência quantitativa de jitter, RF01).
- Observação direta (visual/sonora/mecânica) de cada atuador no instante
  do tick.
- Opcional, se disponível em laboratório: analisador lógico nos pinos de
  servo e buzzer para medir o período real de 1000ms, conforme sugerido na
  página 4 do PDF ("Evidência de Validação").

## O que registrar no relatório

Para cada etapa: se o jitter se manteve dentro do limite, se o atuador
disparou no instante correto, e qualquer efeito colateral observado ao
somar o próximo atuador (ex.: aumento de jitter, atraso perceptível entre
LED e buzzer, servo vibrando fora do tick).
