#!/usr/bin/env python3
"""Atividade 4: metronomo integrado (LED + servomotor + buzzer) a 1 segundo.

Escrito com `gpiozero` (PWMLED, AngularServo, Buzzer), no mesmo estilo dos
tutorials do kit Freenove FNK0054.

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

from gpiozero import AngularServo, Buzzer, PWMLED

from config import (
    BUZZER_PIN,
    LED_PIN,
    SERVO_MAX_PULSE_S,
    SERVO_MIN_PULSE_S,
    SERVO_PIN,
)

PERIOD_S = 1.0  # 60 BPM
BEEP_DURATION_S = 0.08
SERVO_ANGLE_A = 0
SERVO_ANGLE_B = 180


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

    angle = SERVO_ANGLE_A
    try:
        while True:
            cycle_start = time.time()

            beat(led, servo, buzzer, angle)
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
        led.close()
        servo.close()
        buzzer.close()


if __name__ == "__main__":
    main()
