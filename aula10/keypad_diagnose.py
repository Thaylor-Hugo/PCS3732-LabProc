#!/usr/bin/env python3
"""Diagnóstico de fiação do teclado matricial: descobre empiricamente quais
pares de pinos ficam em curto quando uma tecla é pressionada, sem assumir
qual pino é linha e qual é coluna.

Motivo: duas tentativas de mapeamento (fiação original do config.py e o
mapeamento oficial do capítulo do Freenove FNK0054) produziram leituras
inconsistentes (3 de 4 "colunas" retornando a mesma tecla). Isso indica que
a pinagem real da protoboard não é nenhuma das duas suposições — então, em
vez de adivinhar de novo, este script mede o par de pinos real.

Uso: rode o script e vá pressionando e segurando cada tecla (uma de cada
vez, por ~1s) enquanto observa o console. Anote o par (driver, sensor)
impresso para cada tecla física. Ctrl+C para sair.
"""

import time

from gpiozero import DigitalInputDevice, DigitalOutputDevice

PINS = [5, 6, 13, 16, 19, 20, 21, 26]


def main():
    print("[Diagnóstico] pressione e segure cada tecla por ~1s, uma de cada vez.")
    print("[Diagnóstico] Ctrl+C para sair.\n")

    inputs = {pin: DigitalInputDevice(pin, pull_up=True) for pin in PINS}

    try:
        while True:
            for drive_pin in PINS:
                inputs[drive_pin].close()
                driver = DigitalOutputDevice(drive_pin, initial_value=False)

                for sense_pin in PINS:
                    if sense_pin == drive_pin:
                        continue
                    if not inputs[sense_pin].value:
                        print(f"[Diagnóstico] driver={drive_pin}  <->  sensor={sense_pin}")

                driver.close()
                inputs[drive_pin] = DigitalInputDevice(drive_pin, pull_up=True)
            time.sleep(0.15)
    except KeyboardInterrupt:
        print("\nInterrompido pelo usuario.")
    finally:
        for device in inputs.values():
            device.close()


if __name__ == "__main__":
    main()
