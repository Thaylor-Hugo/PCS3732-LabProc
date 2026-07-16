#!/usr/bin/env python3
"""Atividade 3: controle isolado do buzzer (acionamento digital on/off).

Baseado no exemplo "Doorbell" do Capitulo 6 (Buzzer) da documentacao do
kit Freenove FNK0054, usando `gpiozero.Buzzer` (buzzer ativo, acionado por
nivel logico). Se o seu buzzer for passivo (precisa de PWM/frequencia),
troque por `gpiozero.TonalBuzzer` + `Tone(frequencia_hz)`.
"""

import time

from gpiozero import Buzzer

from config import BUZZER_PIN

BEEP_DURATION_S = 0.1
BEEP_COUNT = 5
INTERVAL_S = 0.5


def main():
    buzzer = Buzzer(BUZZER_PIN)

    try:
        for i in range(BEEP_COUNT):
            print(f"[Buzzer] beep {i + 1}/{BEEP_COUNT}")
            buzzer.on()
            time.sleep(BEEP_DURATION_S)
            buzzer.off()
            time.sleep(INTERVAL_S)
    except KeyboardInterrupt:
        print("Interrompido pelo usuario.")
    finally:
        buzzer.close()


if __name__ == "__main__":
    main()
