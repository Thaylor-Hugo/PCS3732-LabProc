#!/usr/bin/env python3
"""Mapeador automático do teclado matricial: para cada tecla do KEYPAD_LAYOUT
(config.py), pede ao usuário para pressionar a tecla correspondente e mede
eletricamente qual PAR de pinos fica em curto, sem assumir nenhuma convenção
de fiação prévia.

O curto entre dois pinos é simétrico (não importa qual lado é acionado como
saída), então cada tecla é registrada como um par NÃO ordenado {pinoA, pinoB}.
No final, a linha i do KEYPAD_LAYOUT é o pino em COMUM entre os pares das 4
teclas daquela linha, e o mesmo vale por coluna. Essa dedução por interseção
é imune à ambiguidade de "quem é driver" que uma medição tecla-a-tecla sozinha
não consegue resolver.

Uso: rode o script e siga as instruções na tela, pressionando e segurando
cada tecla indicada. Ctrl+C interrompe (mapeamento fica incompleto).
"""

import time

from gpiozero import DigitalInputDevice, DigitalOutputDevice

from config import KEYPAD_LAYOUT

PINS = [5, 6, 13, 16, 19, 20, 21, 26]
SETTLE_S = 0.005   # acomodação ao trocar direção do pino (LOW->HIGH via pull-up fraco)
STABLE_S = 0.25    # tempo que o mesmo par precisa se manter estável
POLL_S = 0.02


def scan_once(inputs):
    """Varre todos os pinos como driver e retorna o par (frozenset de 2 pinos)
    em curto encontrado, ou None se nenhuma tecla estiver pressionada."""
    for drive_pin in PINS:
        inputs[drive_pin].close()
        driver = DigitalOutputDevice(drive_pin, initial_value=False)
        time.sleep(SETTLE_S)

        found = None
        for sense_pin in PINS:
            if sense_pin == drive_pin:
                continue
            if inputs[sense_pin].value:  # pull_up=True: gpiozero já inverte, curto = value True
                found = frozenset((drive_pin, sense_pin))

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
    results = {}  # (row_idx, col_idx) -> frozenset({pinoA, pinoB})

    try:
        for row_idx, row in enumerate(KEYPAD_LAYOUT):
            for col_idx, key in enumerate(row):
                input(f"\nPressione e SEGURE a tecla '{key}' e depois tecle Enter aqui...")
                print("  aguardando pressao estavel...")
                pair = wait_for_stable_press(inputs)
                a, b = tuple(pair)
                print(f"  detectado: GPIO{a}  <->  GPIO{b}")
                results[(row_idx, col_idx)] = pair
                wait_for_release(inputs)
                print("  solto.")
    except KeyboardInterrupt:
        print("\nInterrompido pelo usuario - mapeamento incompleto.")
    finally:
        for device in inputs.values():
            device.close()

    print("\n=== Resultado ===")
    if len(results) < n_rows * n_cols:
        print(f"Mapeamento incompleto: {len(results)}/{n_rows * n_cols} teclas medidas.")
        return

    row_pins = []
    col_pins = []
    conflicts = []

    for row_idx in range(n_rows):
        pairs = [results[(row_idx, c)] for c in range(n_cols)]
        common = frozenset.intersection(*pairs)
        if len(common) != 1:
            conflicts.append(
                f"linha {row_idx}: nao ha um unico pino em comum entre as teclas "
                f"{KEYPAD_LAYOUT[row_idx]} (interseccao={sorted(common)})"
            )
            row_pins.append(None)
        else:
            row_pins.append(next(iter(common)))

    for col_idx in range(n_cols):
        pairs = [results[(r, col_idx)] for r in range(n_rows)]
        common = frozenset.intersection(*pairs)
        if len(common) != 1:
            conflicts.append(
                f"coluna {col_idx}: nao ha um unico pino em comum entre as teclas "
                f"{[row[col_idx] for row in KEYPAD_LAYOUT]} (interseccao={sorted(common)})"
            )
            col_pins.append(None)
        else:
            col_pins.append(next(iter(common)))

    if conflicts:
        print("Inconsistencias encontradas (fiacao pode nao ser uma matriz pura "
              "linha x coluna, ou alguma tecla foi mal pressionada):")
        for c in conflicts:
            print(f"  - {c}")
    else:
        print("Mapeamento consistente. Use em config.py:\n")
        print(f"KEYPAD_ROW_PINS = {row_pins}")
        print(f"KEYPAD_COL_PINS = {col_pins}")


if __name__ == "__main__":
    main()
