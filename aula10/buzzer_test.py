#!/usr/bin/env python3
"""Teste isolado: acionamento do buzzer (bipe curto de sucesso e longo de falha)."""

import time

from config import BEEP_FAIL_S, BEEP_SUCCESS_S, BUZZER_PIN
from nonblocking_buzzer import NonBlockingBuzzer


def main():
    buzzer = NonBlockingBuzzer(BUZZER_PIN)

    try:
        print("[Buzzer] bipe curto (sucesso)")
        buzzer.beep(BEEP_SUCCESS_S)
        while buzzer._off_at is not None:
            buzzer.update()
            time.sleep(0.01)

        time.sleep(0.5)

        print("[Buzzer] bipe longo (falha)")
        buzzer.beep(BEEP_FAIL_S)
        while buzzer._off_at is not None:
            buzzer.update()
            time.sleep(0.01)
    except KeyboardInterrupt:
        print("Interrompido pelo usuario.")
    finally:
        buzzer.close()


if __name__ == "__main__":
    main()
