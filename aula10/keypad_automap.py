#!/usr/bin/env python3
"""Mapeador automático do teclado matricial: para cada tecla do KEYPAD_LAYOUT
(config.py), pede ao usuário para pressionar a tecla correspondente e mede
eletricamente qual pino é o driver (saída) e qual é o sensor (entrada), sem
assumir nenhuma convenção de fiação previa.

Ao final, deduz KEYPAD_ROW_PINS/KEYPAD_COL_PINS a partir dos pares medidos e
avisa se houver inconsistência (ex.: a mesma linha do layout usando drivers
diferentes em colunas diferentes, o que indicaria fiação que não é uma
matriz pura linha x coluna).

Uso: rode o script e siga as instruções na tela, pressionando e segurando
cada tecla indicada. Ctrl+C interrompe (mapeamento fica incompleto).
"""

import time

from gpiozero import DigitalInputDevice, DigitalOutputDevice

from config import KEYPAD_LAYOUT

PINS = [5, 6, 13, 16, 19, 20, 21, 26]
SETTLE_S = 0.005   # acomodação ao trocar direção do pino (LOW->HIGH via pull-up fraco)
STABLE_S = 0.25    # tempo que o mesmo par (driver,sensor) precisa se manter estável
POLL_S = 0.02


def scan_once(inputs):
    """Varre todos os pinos como driver e retorna o primeiro par (driver, sensor)
    em curto encontrado, ou None se nenhuma tecla estiver pressionada."""
    for drive_pin in PINS:
        inputs[drive_pin].close()
        driver = DigitalOutputDevice(drive_pin, initial_value=False)
        time.sleep(SETTLE_S)

        found = None
        for sense_pin in PINS:
            if sense_pin == drive_pin:
                continue
            if not inputs[sense_pin].value:
                found = (drive_pin, sense_pin)

        driver.close()
        inputs[drive_pin] = DigitalInputDevice(drive_pin, pull_up=True)
        time.sleep(SETTLE_S)

        if found:
            return found
    return None


def wait_for_stable_press(inputs):
    candidate = None
    candidate_since = None
    while True:
        pair = scan_once(inputs)
        now = time.monotonic()
        if pair is None:
            candidate = None
            candidate_since = None
        elif pair == candidate:
            if now - candidate_since >= STABLE_S:
                return pair
        else:
            candidate = pair
            candidate_since = now
        time.sleep(POLL_S)


def wait_for_release(inputs):
    while scan_once(inputs) is not None:
        time.sleep(POLL_S)


def main():
    n_rows = len(KEYPAD_LAYOUT)
    n_cols = len(KEYPAD_LAYOUT[0])

    inputs = {pin: DigitalInputDevice(pin, pull_up=True) for pin in PINS}
    results = {}

    try:
        for row_idx, row in enumerate(KEYPAD_LAYOUT):
            for col_idx, key in enumerate(row):
                input(f"\nPressione e SEGURE a tecla '{key}' e depois tecle Enter aqui...")
                print("  aguardando pressao estavel...")
                pair = wait_for_stable_press(inputs)
                print(f"  detectado: driver=GPIO{pair[0]}  sensor=GPIO{pair[1]}")
                results[(row_idx, col_idx)] = pair
                wait_for_release(inputs)
                print("  solto.")
    except KeyboardInterrupt:
        print("\nInterrompido pelo usuario - mapeamento incompleto.")
    finally:
        for device in inputs.values():
            device.close()

    row_pins = [None] * n_rows
    col_pins = [None] * n_cols
    conflicts = []

    for (row_idx, col_idx), (driver, sensor) in results.items():
        if row_pins[row_idx] is None:
            row_pins[row_idx] = driver
        elif row_pins[row_idx] != driver:
            conflicts.append(
                f"linha {row_idx} (tecla '{KEYPAD_LAYOUT[row_idx][col_idx]}'): "
                f"driver esperado GPIO{row_pins[row_idx]}, obtido GPIO{driver}"
            )
        if col_pins[col_idx] is None:
            col_pins[col_idx] = sensor
        elif col_pins[col_idx] != sensor:
            conflicts.append(
                f"coluna {col_idx} (tecla '{KEYPAD_LAYOUT[row_idx][col_idx]}'): "
                f"sensor esperado GPIO{col_pins[col_idx]}, obtido GPIO{sensor}"
            )

    print("\n=== Resultado ===")
    if len(results) < n_rows * n_cols:
        print(f"Mapeamento incompleto: {len(results)}/{n_rows * n_cols} teclas medidas.")

    if conflicts:
        print("Inconsistencias encontradas (a fiacao pode nao ser uma matriz pura "
              "linha x coluna, ou uma tecla foi mal pressionada):")
        for c in conflicts:
            print(f"  - {c}")

    if len(results) == n_rows * n_cols and not conflicts:
        print("Mapeamento consistente. Use em config.py:\n")
        print(f"KEYPAD_ROW_PINS = {row_pins}")
        print(f"KEYPAD_COL_PINS = {col_pins}")


if __name__ == "__main__":
    main()
