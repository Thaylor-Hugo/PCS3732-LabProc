#!/usr/bin/env python3
"""Teste isolado: varredura do teclado matricial 4x4 com debounce.

Critério de aceite (RF01): cada pressionamento gera exatamente UM evento
impresso no console, mesmo mantendo a tecla pressionada por mais tempo.
"""

import time

from keypad import Keypad


def main():
    keypad = Keypad()
    print("[Keypad] pressione teclas (Ctrl+C para sair)")

    try:
        while True:
            key = keypad.scan()
            if key is not None:
                print(f"[Keypad] tecla={key}")
            time.sleep(0.01)
    except KeyboardInterrupt:
        print("Interrompido pelo usuario.")
    finally:
        keypad.close()


if __name__ == "__main__":
    main()
