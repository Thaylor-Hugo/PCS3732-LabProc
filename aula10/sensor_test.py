#!/usr/bin/env python3
"""Teste isolado: sensor ultrassônico HC-SR04 (verificação de integridade da tranca).

Usa `gpiozero.DistanceSensor`. Imprime a distância medida e o estado lógico
derivado (TRANCADA/ABERTA) segundo o limiar `SENSOR_LOCKED_MAX_CM`.

Critério de aceite (funil de depuração, Nível 1 — Camada Física): a
transição de estado reflete corretamente 0V/3.3V no pino ECHO ao aproximar
e afastar um objeto do sensor.
"""

import time

from gpiozero import DistanceSensor

from config import SENSOR_ECHO_PIN, SENSOR_LOCKED_MAX_CM, SENSOR_POLL_INTERVAL_S, SENSOR_TRIG_PIN


def state_from_distance(distance_cm):
    return "TRANCADA" if distance_cm <= SENSOR_LOCKED_MAX_CM else "ABERTA"


def main():
    sensor = DistanceSensor(echo=SENSOR_ECHO_PIN, trigger=SENSOR_TRIG_PIN, max_distance=2.0)

    try:
        while True:
            distance_cm = sensor.distance * 100
            print(f"[Sensor] distancia={distance_cm:5.1f}cm estado={state_from_distance(distance_cm)}")
            time.sleep(SENSOR_POLL_INTERVAL_S)
    except KeyboardInterrupt:
        print("Interrompido pelo usuario.")
    finally:
        sensor.close()


if __name__ == "__main__":
    main()
