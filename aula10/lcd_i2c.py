#!/usr/bin/env python3
"""Driver mínimo para display LCD1602 via backpack I2C PCF8574.

Implementa o protocolo HD44780 em modo 4 bits sobre o expansor de I/O
PCF8574, acessado com `smbus2` (nível de aplicação da pilha I2C descrita no
PDF: Python -> smbus -> /dev/i2c-1 -> controlador I2C do BCM2837).

Mapeamento de bits do PCF8574 (padrão dos backpacks LCM1602/YwRobot):
  P0=RS  P1=RW  P2=E  P3=Backlight  P4..P7 = D4..D7 (nibble alto)
"""

import time

from smbus2 import SMBus

# Flags de controle
_RS_CMD = 0x00
_RS_DATA = 0x01
_ENABLE = 0x04
_BACKLIGHT = 0x08

_LINE_ADDR = [0x80, 0xC0]  # endereços DDRAM da linha 1 e 2 (LCD1602)


class LCD1602:
    def __init__(self, bus_num, address, cols=16, rows=2):
        self.bus = SMBus(bus_num)
        self.address = address
        self.cols = cols
        self.rows = rows
        self.backlight = _BACKLIGHT
        self._init_display()

    # -- baixo nível -----------------------------------------------------
    def _write_raw(self, data):
        self.bus.write_byte(self.address, data | self.backlight)

    def _pulse_enable(self, data):
        self._write_raw(data | _ENABLE)
        time.sleep(0.0005)
        self._write_raw(data & ~_ENABLE)
        time.sleep(0.0001)

    def _write4(self, nibble, rs):
        data = (nibble & 0xF0) | rs
        self._write_raw(data)
        self._pulse_enable(data)

    def _send(self, value, rs):
        self._write4(value & 0xF0, rs)
        self._write4((value << 4) & 0xF0, rs)

    def _command(self, value):
        self._send(value, _RS_CMD)

    def _init_display(self):
        time.sleep(0.05)
        # Sequência de reset em modo 4 bits (procedimento padrão HD44780)
        for _ in range(3):
            self._write4(0x30, _RS_CMD)
            time.sleep(0.005)
        self._write4(0x20, _RS_CMD)  # entra em modo 4 bits
        self._command(0x28)  # function set: 4 bits, 2 linhas, fonte 5x8
        self._command(0x0C)  # display on, cursor off, blink off
        self._command(0x06)  # entry mode: incrementa cursor, sem shift
        self.clear()

    # -- API pública -------------------------------------------------------
    def clear(self):
        self._command(0x01)
        time.sleep(0.002)

    def set_backlight(self, on):
        self.backlight = _BACKLIGHT if on else 0x00
        self._write_raw(0x00)

    def write_line(self, text, row):
        row = max(0, min(row, self.rows - 1))
        self._command(_LINE_ADDR[row])
        text = text[: self.cols].ljust(self.cols)
        for char in text:
            self._send(ord(char), _RS_DATA)

    def write_status(self, line1, line2=""):
        """Atualiza as duas linhas do display (usado no feedback de status)."""
        self.write_line(line1, 0)
        self.write_line(line2, 1)

    def close(self):
        self.clear()
        self.set_backlight(False)
        self.bus.close()
