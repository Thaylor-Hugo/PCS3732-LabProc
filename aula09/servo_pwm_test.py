#!/usr/bin/env python3
"""Atividade 2: controle isolado de servomotor via PWM (SG90).

Baseado no exemplo Sweep.py do Capitulo 13 (Servo) da documentacao do kit
Freenove FNK0054, usando `gpiozero.AngularServo`. O sinal e cravado em
50Hz (padrao interno do gpiozero); o deslocamento angular e definido pela
largura de pulso, conforme tabela da pagina 7 do PDF de referencia:
  0.5ms -> 0 graus | 1.5ms -> 90 graus | 2.5ms -> 180 graus
"""

import time

from gpiozero import AngularServo

from config import SERVO_MAX_PULSE_S, SERVO_MIN_PULSE_S, SERVO_PIN


def main():
    servo = AngularServo(
        SERVO_PIN,
        initial_angle=0,
        min_angle=0,
        max_angle=180,
        min_pulse_width=SERVO_MIN_PULSE_S,
        max_pulse_width=SERVO_MAX_PULSE_S,
    )

    try:
        for angle in (0, 45, 90, 135, 180, 90, 0):
            print(f"[Servo] angulo={angle} graus")
            servo.angle = angle
            time.sleep(1)
        servo.detach()  # solta o pulso para nao vibrar em repouso
    except KeyboardInterrupt:
        print("Interrompido pelo usuario.")
    finally:
        servo.close()


if __name__ == "__main__":
    main()
