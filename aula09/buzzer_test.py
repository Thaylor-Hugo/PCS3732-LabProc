#!/usr/bin/env python3
"""Atividade 3: controle isolado do buzzer (acionamento digital on/off).

O buzzer usado e passivo/ativo simples, acionado por nivel logico
(sem necessidade de PWM), conforme "Sinal Digital" no diagrama de
roteamento (pagina 5 do PDF de referencia).
"""

import time

import RPi.GPIO as GPIO

from config import BUZZER_PIN

BEEP_DURATION_S = 0.1
BEEP_COUNT = 5
INTERVAL_S = 0.5


def beep(duration_s=BEEP_DURATION_S):
    GPIO.output(BUZZER_PIN, GPIO.HIGH)
    time.sleep(duration_s)
    GPIO.output(BUZZER_PIN, GPIO.LOW)


def main():
    GPIO.setmode(GPIO.BCM)
    GPIO.setup(BUZZER_PIN, GPIO.OUT, initial=GPIO.LOW)

    try:
        for i in range(BEEP_COUNT):
            print(f"[Buzzer] beep {i + 1}/{BEEP_COUNT}")
            beep()
            time.sleep(INTERVAL_S)
    except KeyboardInterrupt:
        print("Interrompido pelo usuario.")
    finally:
        GPIO.cleanup()


if __name__ == "__main__":
    main()
