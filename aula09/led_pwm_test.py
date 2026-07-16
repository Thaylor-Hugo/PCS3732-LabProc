#!/usr/bin/env python3
"""Atividade 1: controle de LED via PWM testando diversas frequencias.

Baseado no exemplo do Capitulo 4 (Analog & PWM) da documentacao do kit
Freenove FNK0054, usando `gpiozero.PWMLED`. Varre o duty cycle (led.value,
0.0 a 1.0) em cada frequencia, permitindo observar o efeito da frequencia
sobre o brilho (persistencia da visao, ver pagina 7 do PDF de referencia).
"""

import time

from gpiozero import PWMLED

from config import LED_PIN

FREQUENCIES_HZ = [1, 5, 50, 100, 1000]
STEP_DELAY_S = 0.01


def sweep(led):
    for b in range(0, 101, 1):
        led.value = b / 100.0
        time.sleep(STEP_DELAY_S)
    for b in range(100, -1, -1):
        led.value = b / 100.0
        time.sleep(STEP_DELAY_S)


def main():
    try:
        for freq in FREQUENCIES_HZ:
            print(f"[LED] Testando frequencia = {freq} Hz")
            led = PWMLED(LED_PIN, initial_value=0, frequency=freq)
            sweep(led)
            led.close()
    except KeyboardInterrupt:
        print("Interrompido pelo usuario.")


if __name__ == "__main__":
    main()
