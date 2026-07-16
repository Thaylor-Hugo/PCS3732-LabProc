#!/usr/bin/env python3
"""Desafio: metronomo com configuracao de frequencia via botoes fisicos.

Arquitetura (pagina 6 do PDF de referencia): a leitura dos botoes roda em
callbacks de interrupcao (thread secundaria, nativa do RPi.GPIO) que apenas
atualizam a variavel global de BPM. O loop principal (thread do metronomo)
nunca bloqueia esperando pelos botoes -- ele apenas le o BPM vigente a cada
ciclo, preservando a temporizacao de 1Hz mesmo durante o ajuste (RF02).

RF02 (Controle BPM): incremento/decremento linear sem interromper o loop.
RNF01 (Debouncing Fisico): `bouncetime=200` no add_event_detect ignora
cliques transientes por 200ms apos o primeiro evento valido.
"""

import threading
import time

import RPi.GPIO as GPIO

from config import (
    BPM_DEFAULT,
    BPM_MAX,
    BPM_MIN,
    BPM_STEP,
    BUTTON_BOUNCETIME_MS,
    BUTTON_DOWN_PIN,
    BUTTON_UP_PIN,
    BUZZER_PIN,
    LED_PIN,
    SERVO_FREQ_HZ,
    SERVO_PIN,
)

BEEP_DURATION_S = 0.08
LED_PULSE_DUTY = 100
SERVO_ANGLE_A = 0
SERVO_ANGLE_B = 180

_bpm_lock = threading.Lock()
_bpm = BPM_DEFAULT


def get_period_s():
    with _bpm_lock:
        return 60.0 / _bpm


def on_button_up(_channel):
    global _bpm
    with _bpm_lock:
        _bpm = min(BPM_MAX, _bpm + BPM_STEP)
        print(f"> BPM alterado para {_bpm}")


def on_button_down(_channel):
    global _bpm
    with _bpm_lock:
        _bpm = max(BPM_MIN, _bpm - BPM_STEP)
        print(f"> BPM alterado para {_bpm}")


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


def setup_buttons():
    GPIO.setup(BUTTON_UP_PIN, GPIO.IN, pull_up_down=GPIO.PUD_DOWN)
    GPIO.setup(BUTTON_DOWN_PIN, GPIO.IN, pull_up_down=GPIO.PUD_DOWN)
    GPIO.add_event_detect(
        BUTTON_UP_PIN, GPIO.RISING,
        callback=on_button_up, bouncetime=BUTTON_BOUNCETIME_MS,
    )
    GPIO.add_event_detect(
        BUTTON_DOWN_PIN, GPIO.RISING,
        callback=on_button_down, bouncetime=BUTTON_BOUNCETIME_MS,
    )


def main():
    GPIO.setmode(GPIO.BCM)
    GPIO.setup(LED_PIN, GPIO.OUT)
    GPIO.setup(SERVO_PIN, GPIO.OUT)
    GPIO.setup(BUZZER_PIN, GPIO.OUT, initial=GPIO.LOW)
    setup_buttons()

    led_pwm = GPIO.PWM(LED_PIN, 1000)
    led_pwm.start(0)
    servo_pwm = GPIO.PWM(SERVO_PIN, SERVO_FREQ_HZ)
    servo_pwm.start(0)

    angle = SERVO_ANGLE_A
    try:
        while True:
            cycle_start = time.time()
            period_s = get_period_s()

            beat(led_pwm, servo_pwm, angle)
            angle = SERVO_ANGLE_B if angle == SERVO_ANGLE_A else SERVO_ANGLE_A

            drift_time = time.time() - cycle_start
            sleep_time = period_s - drift_time
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
