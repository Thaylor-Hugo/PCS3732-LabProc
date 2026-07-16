#!/usr/bin/env python3
"""Atividade 2: controle isolado de servomotor via PWM (SG90).

Sinal cravado em 50Hz (periodo de 20ms); o deslocamento angular e definido
pela largura de pulso, conforme tabela da pagina 7 do PDF de referencia:
  1.0ms (~5.0% DC)  ->   0 graus
  1.5ms (~7.5% DC)  ->  90 graus
  2.0ms (~10.0% DC) -> 180 graus
"""

import time

import RPi.GPIO as GPIO

from config import SERVO_FREQ_HZ, SERVO_PIN


def angle_to_duty_cycle(angle_deg):
    angle_deg = max(0, min(180, angle_deg))
    pulse_ms = 1.0 + (angle_deg / 180.0) * 1.0  # 1.0ms .. 2.0ms
    return (pulse_ms / 20.0) * 100.0  # periodo de 20ms a 50Hz


def main():
    GPIO.setmode(GPIO.BCM)
    GPIO.setup(SERVO_PIN, GPIO.OUT)

    pwm = GPIO.PWM(SERVO_PIN, SERVO_FREQ_HZ)
    pwm.start(0)

    try:
        for angle in (0, 45, 90, 135, 180, 90, 0):
            duty = angle_to_duty_cycle(angle)
            print(f"[Servo] angulo={angle} graus -> duty_cycle={duty:.2f}%")
            pwm.ChangeDutyCycle(duty)
            time.sleep(1)
        pwm.ChangeDutyCycle(0)  # solta o pulso para nao vibrar em repouso
    except KeyboardInterrupt:
        print("Interrompido pelo usuario.")
    finally:
        pwm.stop()
        GPIO.cleanup()


if __name__ == "__main__":
    main()
