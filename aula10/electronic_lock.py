#!/usr/bin/env python3
"""Fechadura eletrônica (versão normal) — Aula 10, PCS3732.

Integra teclado matricial (entrada de senha), LCD I2C (feedback de status),
buzzer (feedback sonoro) e sensor ultrassônico (integridade física da
tranca), seguindo a máquina de estados não-bloqueante do PDF de referência:

    Idle -> Evento de Entrada -> Processamento -> (Sucesso | Falha)

Nenhum componente usa `time.sleep()` de forma bloqueante dentro do loop
principal: o buzzer é temporizado por `NonBlockingBuzzer`, e o
teclado/sensor são apenas *polled* a cada iteração (RNF1 — "bloqueio
temporário sem travamento do SO").

A senha é comparada em texto plano (`config.DEFAULT_PASSWORD`) — a versão
com hashing/comparação em tempo constante e detecção de violação do sensor
está em `electronic_lock_desafio.py`.
"""

import time

from config import (
    AUTO_RELOCK_S,
    BEEP_FAIL_S,
    BEEP_SUCCESS_S,
    BUZZER_PIN,
    CLEAR_KEY,
    COOLDOWN_S,
    DEFAULT_PASSWORD,
    LCD_COLS,
    LCD_I2C_ADDR,
    LCD_I2C_BUS,
    LCD_ROWS,
    MAIN_LOOP_INTERVAL_S,
    MAX_FAILED_ATTEMPTS,
    PASSWORD_MAX_LEN,
    SENSOR_ECHO_PIN,
    SENSOR_LOCKED_MAX_CM,
    SENSOR_POLL_INTERVAL_S,
    SENSOR_TRIG_PIN,
    SUBMIT_KEY,
)
from gpiozero import DistanceSensor
from keypad import Keypad
from lcd_i2c import LCD1602
from nonblocking_buzzer import NonBlockingBuzzer

STATE_LOCKED = "LOCKED"
STATE_UNLOCKED = "UNLOCKED"
STATE_COOLDOWN = "COOLDOWN"
STATE_ALERT = "ALERT"


class ElectronicLock:
    def __init__(self):
        self.keypad = Keypad()
        self.lcd = LCD1602(LCD_I2C_BUS, LCD_I2C_ADDR, cols=LCD_COLS, rows=LCD_ROWS)
        self.buzzer = NonBlockingBuzzer(BUZZER_PIN)
        self.sensor = DistanceSensor(echo=SENSOR_ECHO_PIN, trigger=SENSOR_TRIG_PIN, max_distance=2.0)

        self.state = STATE_LOCKED
        self.buffer = ""
        self.failed_attempts = 0
        self.state_until = None     # timestamp de expiração do estado atual (UNLOCKED/COOLDOWN)
        self._next_sensor_poll = 0.0
        self._physical_locked = True

        self._render()

    # -- sensor ---------------------------------------------------------
    def _poll_sensor(self, now):
        if now < self._next_sensor_poll:
            return
        self._next_sensor_poll = now + SENSOR_POLL_INTERVAL_S

        distance_cm = self.sensor.distance * 100
        self._physical_locked = distance_cm <= SENSOR_LOCKED_MAX_CM

        # RF3: se o sistema acredita estar TRANCADO mas o sensor detecta a
        # lingueta ausente (abertura forçada), dispara alerta.
        if self.state == STATE_LOCKED and not self._physical_locked:
            self._enter_alert()

    def _enter_alert(self):
        self.state = STATE_ALERT
        self.buffer = ""
        self.buzzer.beep(BEEP_FAIL_S)
        self._render()

    # -- teclado ----------------------------------------------------------
    def _handle_key(self, key, now):
        if self.state in (STATE_COOLDOWN,):
            return  # ignora entrada durante o bloqueio temporário (RNF1)

        if self.state == STATE_ALERT:
            # Qualquer tecla reconhece o alerta e volta ao estado de entrada,
            # desde que a integridade física já tenha sido restaurada.
            if key == SUBMIT_KEY and self._physical_locked:
                self.state = STATE_LOCKED
                self._render()
            return

        if key == CLEAR_KEY:
            self.buffer = self.buffer[:-1]
        elif key == SUBMIT_KEY:
            self._submit(now)
        elif key.isdigit() and len(self.buffer) < PASSWORD_MAX_LEN:
            self.buffer += key
        self._render()

    def _submit(self, now):
        if self.buffer == DEFAULT_PASSWORD:
            self.failed_attempts = 0
            self.state = STATE_UNLOCKED
            self.state_until = now + AUTO_RELOCK_S
            self.buzzer.beep(BEEP_SUCCESS_S)
        else:
            self.failed_attempts += 1
            self.buzzer.beep(BEEP_FAIL_S)
            if self.failed_attempts >= MAX_FAILED_ATTEMPTS:
                self.state = STATE_COOLDOWN
                self.state_until = now + COOLDOWN_S
            else:
                self.state = STATE_LOCKED
        self.buffer = ""

    # -- transições dependentes de tempo -----------------------------------
    def _update_timed_state(self, now):
        if self.state_until is None or now < self.state_until:
            return
        self.state_until = None
        if self.state == STATE_UNLOCKED:
            self.state = STATE_LOCKED
        elif self.state == STATE_COOLDOWN:
            self.failed_attempts = 0
            self.state = STATE_LOCKED
        self._render()

    # -- LCD ----------------------------------------------------------------
    def _render(self):
        if self.state == STATE_LOCKED:
            masked = "*" * len(self.buffer)
            self.lcd.write_status("STATUS: Trancada", masked or "Digite a senha")
        elif self.state == STATE_UNLOCKED:
            self.lcd.write_status("STATUS: Aberto", "Bem-vindo!")
        elif self.state == STATE_COOLDOWN:
            remaining = max(0, int((self.state_until or 0) - time.monotonic()))
            self.lcd.write_status("Bloqueado", f"Aguarde {remaining}s")
        elif self.state == STATE_ALERT:
            self.lcd.write_status("ALERTA!", "Violacao detect.")

    def run(self):
        try:
            while True:
                now = time.monotonic()

                key = self.keypad.scan()
                if key is not None:
                    self._handle_key(key, now)

                self._poll_sensor(now)
                self._update_timed_state(now)
                self.buzzer.update()

                # Recalcula o LCD durante o cooldown para mostrar a contagem
                # regressiva (não é um novo estado, só refresh de exibição).
                if self.state == STATE_COOLDOWN:
                    self._render()

                time.sleep(MAIN_LOOP_INTERVAL_S)
        except KeyboardInterrupt:
            print("Interrompido pelo usuario.")
        finally:
            self.close()

    def close(self):
        self.keypad.close()
        self.buzzer.close()
        self.sensor.close()
        self.lcd.close()


if __name__ == "__main__":
    ElectronicLock().run()
