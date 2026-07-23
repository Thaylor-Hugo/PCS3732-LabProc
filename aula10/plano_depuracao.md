# Plano de Depuração — Fechadura Eletrônica RPi3

Segue o "funil de isolamento de falhas" de 3 níveis do PDF de referência:
Camada Física -> Camada de Sistema/Driver -> Camada Lógica. Sempre
depurar de baixo para cima — não adianta investigar a lógica de estados se
o hardware ainda não está eletricamente correto.

## Nível 1 — Camada Física

- Conferir tensões com multímetro (ou inspeção visual dos jumpers): 3.3V
  nas linhas/colunas do teclado e no ECHO do HC-SR04 (**usar divisor
  resistivo no ECHO**, que opera em 5V — ligar direto ao GPIO pode
  danificar o Raspberry Pi 3), 5V no TRIG e no buzzer.
- Confirmar que os jumpers do barramento I2C (SDA=GPIO2, SCL=GPIO3) estão
  firmes e que o LCD tem alimentação (retroiluminação acesa).
- Sintoma típico neste nível: leitura sempre em um único estado (ex.:
  sensor sempre "ABERTA") — geralmente cabo solto ou GND não-comum entre
  os módulos.

## Nível 2 — Camada de Sistema/Driver

- `sudo raspi-config` -> Interface Options -> habilitar I2C.
- `i2cdetect -y 1` deve listar o endereço do LCD (ex.: `27`). Se não
  aparecer, o problema é de driver/fiação, não de código Python.
- `dmesg | tail` após conectar/energizar cada módulo, procurando erros de
  barramento I2C ou GPIO já em uso por outro processo.
- Rodar cada `*_test.py` isoladamente antes de suspeitar do script de
  integração (`electronic_lock.py`).

## Nível 3 — Camada Lógica

- Logs de estado no console a cada transição (`_render`, `_submit`,
  `_enter_alert`), com timestamp implícito do `print()`, para confirmar
  que o fluxo Idle -> Evento de Entrada -> Processamento -> Sucesso/Falha
  ocorre na ordem esperada.
- Se o teclado "engasgar" (eventos perdidos) ao integrar o buzzer: sintoma
  de `sleep()` bloqueante reintroduzido por engano — verificar que todo
  temporizador de bipe passa por `NonBlockingBuzzer.update()`.
- Se o LCD atualizar com atraso perceptível: medir o tempo entre a tecla
  `#` e a chamada de `_render()` — checar se `MAIN_LOOP_INTERVAL_S` não
  foi aumentado além do necessário.

## Registro no relatório (exemplo de formato)

> "O teclado registrava múltiplas entradas para o mesmo toque (bouncing).
> Causa raiz: o debounce comparava apenas a tecla lida, sem exigir
> estabilidade por um intervalo mínimo. Refinamos `Keypad.scan()` para só
> confirmar uma tecla após `KEYPAD_DEBOUNCE_S` (50ms) de leitura estável."

Cada erro documentado dessa forma (sintoma -> causa raiz -> correção) conta
como evidência de engenharia sólida no relatório final.
