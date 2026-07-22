# Planos de Integração e Depuração — Resumo

*(versão condensada de `plano_integracao.md` e `plano_depuracao.md`)*

## Plano de Integração (incremental, não big-bang)

| Etapa | O que é adicionado | Critério para avançar |
|---|---|---|
| 1 | Loop de 1Hz puro, sem atuadores (só `print` do tick) | Jitter < 5ms por 30 ciclos seguidos |
| 2 | + LED | LED pulsa no tick, jitter mantido |
| 3 | + Buzzer | Beep sincronizado ao LED, jitter mantido |
| 4 | + Servo | Movimento sincronizado ao beep/LED |
| 5 | + Alternância de ângulo (tic-tac) | Troca de estado não aumenta o `drift_time` |
| 6 (desafio) | + Botões físicos (thread separada) | BPM muda sem travar/atrasar a batida |

Regra geral: um componente por vez, sempre validado no hardware antes do próximo.

## Plano de Depuração (isolar antes de corrigir)

| Sintoma | Causa provável | Como investigar |
|---|---|---|
| Jitter > 5ms | Corpo do loop lento; PWM por software sofrendo do escalonador do Linux (RPi3 não é RTOS) | Ler `drift_time`/`sleep_time` já impressos; remover atuadores um a um até o jitter cair |
| Atuadores fora de sincronia | `sleep()` extra entre chamadas; custo diferente entre `led.value`, `servo.angle`, `buzzer.on()` | Cronometrar cada linha de `beat()` com `time.time()` |
| Botão dispara múltiplas vezes | Bouncing mecânico não filtrado | Testar botão isolado; aumentar `BUTTON_BOUNCE_TIME_S` |
| Servo não bate 0°/180° certos | `min_pulse_width`/`max_pulse_width` não calibrados para o SG90 específico | Ajustar em `config.py` em passos de 0.05ms via `servo_pwm_test.py` |

Sempre que um sintoma aparecer na integração: voltar para a última etapa estável e reintroduzir só o componente suspeito, em vez de depurar com os três atuadores rodando juntos.

**No relatório final:** para cada sintoma realmente observado no hardware, registrar causa raiz, método de isolamento, correção aplicada e resultado (antes/depois). Se uma categoria não apresentar problema, registrar "testado, não reproduzido".
