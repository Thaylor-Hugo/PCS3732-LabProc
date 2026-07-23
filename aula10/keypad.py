#!/usr/bin/env python3
"""Driver de varredura do teclado matricial 4x4, com debounce por software.

Usa `gpiozero.DigitalOutputDevice` para as colunas e
`gpiozero.DigitalInputDevice` (pull-up interno) para as linhas — mesma
estratégia de varredura do exemplo de referência que funciona no hardware
(`MatrixKeypad.py`/`Keypad.py`, freenove FNK0054): apenas UMA coluna é
colocada em nível baixo por vez; se alguma linha também estiver em nível
baixo, a tecla correspondente foi pressionada.

Um evento só é reportado uma vez por pressionamento: a tecla tem que voltar
a "solta" antes de gerar um novo evento (`_last_key`), e uma tecla recém
detectada só é confirmada após `debounce_s` permanecer estável (RF01/funil
de depuração do PDF: "delay de 50ms na varredura").
"""

import time

from gpiozero import DigitalInputDevice, DigitalOutputDevice

from config import KEYPAD_COL_PINS, KEYPAD_DEBOUNCE_S, KEYPAD_LAYOUT, KEYPAD_ROW_PINS


class Keypad:
    def __init__(self, row_pins=None, col_pins=None, layout=None, debounce_s=None):
        self.rows = [DigitalInputDevice(pin, pull_up=True) for pin in (row_pins or KEYPAD_ROW_PINS)]
        self.cols = [DigitalOutputDevice(pin, initial_value=True) for pin in (col_pins or KEYPAD_COL_PINS)]
        self.layout = layout or KEYPAD_LAYOUT
        self.debounce_s = debounce_s if debounce_s is not None else KEYPAD_DEBOUNCE_S

        self._last_key = None          # última tecla ainda pressionada (evita repetição)
        self._candidate_key = None     # tecla vista na leitura anterior, aguardando debounce
        self._candidate_since = 0.0

    def _raw_scan(self):
        """Retorna a tecla atualmente pressionada (ou None), sem debounce."""
        for col_index, col in enumerate(self.cols):
            col.off()  # nível baixo apenas na coluna sob teste
            for row_index, row in enumerate(self.rows):
                if row.value:  # pull_up=True: gpiozero já inverte, pressionado = value True
                    col.on()
                    return self.layout[row_index][col_index]
            col.on()
        return None

    def scan(self):
        """Retorna a tecla recém-confirmada (debounced) ou None.

        Só emite uma tecla quando ela transita de "solta" para "pressionada
        e estável por >= debounce_s"; enquanto o dedo permanece na tecla,
        `scan()` continua retornando None (sem repetição automática).
        """
        raw = self._raw_scan()
        now = time.monotonic()

        if raw is None:
            self._last_key = None
            self._candidate_key = None
            return None

        if raw != self._candidate_key:
            self._candidate_key = raw
            self._candidate_since = now
            return None

        stable = (now - self._candidate_since) >= self.debounce_s
        if stable and raw != self._last_key:
            self._last_key = raw
            return raw
        return None

    def close(self):
        for device in (*self.rows, *self.cols):
            device.close()
