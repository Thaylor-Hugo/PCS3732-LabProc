#!/usr/bin/env python3
"""Buzzer com temporização não-bloqueante.

O PDF de referência aponta como ameaça de integração o uso de `sleep()`
para controlar a duração do bipe: isso congela a varredura do teclado e a
leitura do sensor enquanto o buzzer soa. Aqui o desligamento é agendado
como um timestamp (`_off_at`) verificado a cada iteração do loop principal
via `update()` — o laço nunca bloqueia esperando o buzzer.
"""

import time

from gpiozero import Buzzer


class NonBlockingBuzzer:
    def __init__(self, pin):
        self._buzzer = Buzzer(pin)
        self._off_at = None

    def beep(self, duration_s):
        self._buzzer.on()
        self._off_at = time.monotonic() + duration_s

    def update(self):
        """Chamar a cada iteração do loop principal."""
        if self._off_at is not None and time.monotonic() >= self._off_at:
            self._buzzer.off()
            self._off_at = None

    def close(self):
        self._buzzer.close()
