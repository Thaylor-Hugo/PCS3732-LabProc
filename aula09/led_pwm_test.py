#!/usr/bin/env python3
"""Atividade 1: controle de LED via PWM testando diversas frequencias.

Varre o duty cycle de 0% a 100% (e volta) em cada frequencia da lista,
permitindo observar visualmente o efeito da frequencia sobre o brilho
(persistencia da visao, ver pagina 7 do PDF de referencia).
"""

import time

import RPi.GPIO as GPIO

from config import LED_PIN

FREQUENCIES_HZ = [1, 5, 50, 100, 1000]
STEP_DELAY_S = 0.02
DUTY_STEP = 2


def sweep(pwm, seconds_per_direction=1.0):
    steps = int(seconds_per_direction / STEP_DELAY_S)
    duty_step = 100 / steps
    duty = 0.0
    for _ in range(steps):
        duty += duty_step
        pwm.ChangeDutyCycle(min(duty, 100))
        time.sleep(STEP_DELAY_S)
    for _ in range(steps):
        duty -= duty_step
        pwm.ChangeDutyCycle(max(duty, 0))
        time.sleep(STEP_DELAY_S)


def main():
    GPIO.setmode(GPIO.BCM)
    GPIO.setup(LED_PIN, GPIO.OUT)

    try:
        for freq in FREQUENCIES_HZ:
            print(f"[LED] Testando frequencia = {freq} Hz")
            pwm = GPIO.PWM(LED_PIN, freq)
            pwm.start(0)
            sweep(pwm)
            pwm.stop()
    except KeyboardInterrupt:
        print("Interrompido pelo usuario.")
    finally:
        GPIO.cleanup()


if __name__ == "__main__":
    main()
