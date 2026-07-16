#!/usr/bin/env python3
"""Atividade 4: metronomo integrado (LED + servomotor + buzzer) a 1 segundo.

Requisito RF01 (Temporizacao 1Hz): ciclo exato de 1000ms com jitter < 5ms.

Estrategia de temporizacao (pagina 2 do PDF de referencia): em vez de
`time.sleep(1)` puro -- que acumula erro (drift) porque nao contabiliza o
tempo gasto executando o corpo do loop -- mede-se o tempo de execucao do
ciclo (drift_time) e dorme apenas o restante ate completar 1 segundo:

    drift_time = tempo_execucao_do_laco
    sleep(1.0 - drift_time)

Cada batida: o servo alterna entre 0 e 180 graus (efeito "tic-tac"), o LED
acende e o buzzer emite um beep curto.
"""

import time

import RPi.GPIO as GPIO

from config import BUZZER_PIN, LED_PIN, SERVO_FREQ_HZ, SERVO_PIN

PERIOD_S = 1.0  # 60 BPM
BEEP_DURATION_S = 0.08
LED_PULSE_DUTY = 100
SERVO_ANGLE_A = 0
SERVO_ANGLE_B = 180


def angle_to_duty_cycle(angle_deg):
    pulse_ms = 1.0 + (angle_deg / 180.0) * 1.0
    return (pulse_ms / 20.0) * 100.0


def beat(led_pwm, servo_pwm, angle):
    GPIO.output(BUZZER_PIN, GPIO.HIGH)
    led_pwm.ChangeDutyCycle(LED_PULSE_DUTY)
    servo_pwm.ChangeDutyCycle(angle_to_duty_cycle(angle))
    time.sleep(BEEP_DURATION_S)
    GPIO.output(BUZZER_PIN, GPIO.LOW)
    led_pwm.ChangeDutyCycle(0)


def main():
    GPIO.setmode(GPIO.BCM)
    GPIO.setup(LED_PIN, GPIO.OUT)
    GPIO.setup(SERVO_PIN, GPIO.OUT)
    GPIO.setup(BUZZER_PIN, GPIO.OUT, initial=GPIO.LOW)

    led_pwm = GPIO.PWM(LED_PIN, 1000)
    led_pwm.start(0)
    servo_pwm = GPIO.PWM(SERVO_PIN, SERVO_FREQ_HZ)
    servo_pwm.start(0)

    angle = SERVO_ANGLE_A
    try:
        while True:
            cycle_start = time.time()

            beat(led_pwm, servo_pwm, angle)
            angle = SERVO_ANGLE_B if angle == SERVO_ANGLE_A else SERVO_ANGLE_A

            drift_time = time.time() - cycle_start
            sleep_time = PERIOD_S - drift_time
            print(f"[Metronomo] drift={drift_time * 1000:.1f}ms "
                  f"sleep={max(sleep_time, 0) * 1000:.1f}ms")
            if sleep_time > 0:
                time.sleep(sleep_time)
    except KeyboardInterrupt:
        print("Interrompido pelo usuario.")
    finally:
        led_pwm.stop()
        servo_pwm.stop()
        GPIO.cleanup()


if __name__ == "__main__":
    main()
