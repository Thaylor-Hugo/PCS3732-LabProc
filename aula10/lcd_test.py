#!/usr/bin/env python3
"""Teste isolado: display LCD1602 via I2C.

Critério de aceite (funil de depuração, Nível 2 — Camada de Sistema):
`i2cdetect -y 1` encontra o display no endereço configurado, e este script
escreve "Hello World" nas duas linhas do LCD.
"""

import time

from config import LCD_COLS, LCD_I2C_ADDR, LCD_I2C_BUS, LCD_ROWS
from lcd_i2c import LCD1602


def main():
    lcd = LCD1602(LCD_I2C_BUS, LCD_I2C_ADDR, cols=LCD_COLS, rows=LCD_ROWS)

    try:
        lcd.write_status("Hello World", "LCD1602 via I2C")
        print("[LCD] mensagem escrita, aguardando 5s")
        time.sleep(5)
    except KeyboardInterrupt:
        print("Interrompido pelo usuario.")
    finally:
        lcd.close()


if __name__ == "__main__":
    main()
