#!/usr/bin/env python3
"""Desafio: metronomo com configuracao de frequencia via botoes fisicos.

Escrito com `gpiozero` (PWMLED, AngularServo, Buzzer, Button), no mesmo
estilo dos tutoriais do kit Freenove FNK0054 (ex.: Capitulo 3 Buttons & LEDs).

Arquitetura (pagina 6 do PDF de referencia): a leitura dos botoes roda em
callbacks de interrupcao do gpiozero (`when_pressed`), executados em thread
propria da biblioteca, que apenas atualizam a variavel global de BPM. O
loop principal (thread do metronomo) nunca bloqueia esperando pelos botoes
-- ele apenas le o BPM vigente a cada ciclo, preservando a temporizacao de
1Hz mesmo durante o ajuste (RF02).

RF02 (Controle BPM): incremento/decremento linear sem interromper o loop.
RNF01 (Debouncing Fisico): `bounce_time` do gpiozero ignora cliques
transientes por 200ms apos o primeiro evento valido.
"""

import threading
import time

from gpiozero import AngularServo, Button, Buzzer, PWMLED

from config import (
    BPM_DEFAULT,
    BPM_MAX,
    BPM_MIN,
    BPM_STEP,
    BUTTON_BOUNCE_TIME_S,
    BUTTON_DOWN_PIN,
    BUTTON_UP_PIN,
    BUZZER_PIN,
    LED_PIN,
    SERVO_MAX_PULSE_S,
    SERVO_MIN_PULSE_S,
    SERVO_PIN,
)

BEEP_DURATION_S = 0.08
SERVO_ANGLE_A = 0
SERVO_ANGLE_B = 180

_bpm_lock = threading.Lock()
_bpm = BPM_DEFAULT


def get_period_s():
    with _bpm_lock:
        return 60.0 / _bpm


def on_button_up():
    global _bpm
    with _bpm_lock:
        _bpm = min(BPM_MAX, _bpm + BPM_STEP)
        print(f"> BPM alterado para {_bpm}")


def on_button_down():
    global _bpm
    with _bpm_lock:
        _bpm = max(BPM_MIN, _bpm - BPM_STEP)
        print(f"> BPM alterado para {_bpm}")


def beat(led, servo, buzzer, angle):
    buzzer.on()
    led.value = 1.0
    servo.angle = angle
    time.sleep(BEEP_DURATION_S)
    buzzer.off()
    led.value = 0.0


def main():
    led = PWMLED(LED_PIN, initial_value=0, frequency=1000)
    servo = AngularServo(
        SERVO_PIN,
        initial_angle=SERVO_ANGLE_A,
        min_angle=0,
        max_angle=180,
        min_pulse_width=SERVO_MIN_PULSE_S,
        max_pulse_width=SERVO_MAX_PULSE_S,
    )
    buzzer = Buzzer(BUZZER_PIN)

    button_up = Button(BUTTON_UP_PIN, bounce_time=BUTTON_BOUNCE_TIME_S)
    button_down = Button(BUTTON_DOWN_PIN, bounce_time=BUTTON_BOUNCE_TIME_S)
    button_up.when_pressed = on_button_up
    button_down.when_pressed = on_button_down

    angle = SERVO_ANGLE_A
    try:
        while True:
            cycle_start = time.time()
            period_s = get_period_s()

            beat(led, servo, buzzer, angle)
            angle = SERVO_ANGLE_B if angle == SERVO_ANGLE_A else SERVO_ANGLE_A

            drift_time = time.time() - cycle_start
            sleep_time = period_s - drift_time
            if sleep_time > 0:
                time.sleep(sleep_time)
    except KeyboardInterrupt:
        print("Interrompido pelo usuario.")
    finally:
        led.close()
        servo.close()
        buzzer.close()
        button_up.close()
        button_down.close()


if __name__ == "__main__":
    main()
