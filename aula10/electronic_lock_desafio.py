#!/usr/bin/env python3
"""Fechadura eletrônica — versão DESAFIO (Aula 10, item 6: análise de segurança).

Estende `electronic_lock.py` com mitigações para as vulnerabilidades
discutidas no relatório:

1. **Senha em hash (SHA-256 + salt)**: a senha nunca é comparada nem
   armazenada em texto plano, mitigando exposição em memória/logs.
2. **Comparação em tempo constante** (`hmac.compare_digest`): evita
   timing attacks facilitados pelo scheduler não-determinístico do Linux
   no RPi3 (ver PDF, "Criptografia e Restrições de Hardware").
3. **Detecção de spoofing do sensor** ("Passo 2" do vetor de ataque
   exemplificado: jumper/imã forçando o estado FECHADO): leituras
   fisicamente implausíveis (distância estática por tempo demais, ou fora
   da faixa do HC-SR04) são sinalizadas como suspeitas antes de serem
   confiadas.
4. **Log tamper-evident (hash chain)**: cada evento de segurança é
   encadeado por hash ao evento anterior (`_chain_hash`), de forma que
   qualquer edição retroativa do arquivo de log quebra a cadeia — mesma
   ideia de um blockchain simplificado, viável em Python puro (`hashlib`)
   tanto no RPi3 quanto, com mais esforço de RAM, em um ESP32.
5. **Cooldown com backoff exponencial**: cada novo bloqueio dobra a
   duração do anterior, penalizando tentativas de força bruta repetidas.
"""

import hashlib
import hmac
import json
import time
from pathlib import Path

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

# Em produção o salt deve ser gerado por dispositivo (`os.urandom`) e
# persistido fora do código-fonte; aqui é fixo apenas para reprodutibilidade
# didática do experimento (ver checklist do relatório, item "Reprodutibilidade").
PASSWORD_SALT = b"aula10-fnk0054-salt"
PASSWORD_HASH = hashlib.sha256(PASSWORD_SALT + DEFAULT_PASSWORD.encode()).hexdigest()

LOG_PATH = Path(__file__).parent / "security_log.jsonl"
GENESIS_HASH = "0" * 64

# Leituras do HC-SR04 fora desta faixa são fisicamente impossíveis para o
# sensor (datasheet: 2cm-400cm) e indicam sinal manipulado/curto-circuito.
SENSOR_VALID_RANGE_CM = (2.0, 400.0)
# Nº de leituras idênticas (variação < 0.05cm) consideradas "estáticas
# demais para serem reais" — ruído ambiente natural sempre introduz
# pequena variação; um valor fixo sugere jumper/ímã forçando o pino.
SENSOR_STATIC_STREAK_LIMIT = 40  # ~10s a SENSOR_POLL_INTERVAL_S=0.25s


def hash_password(plain_text):
    return hashlib.sha256(PASSWORD_SALT + plain_text.encode()).hexdigest()


def verify_password(plain_text):
    return hmac.compare_digest(hash_password(plain_text), PASSWORD_HASH)


class SecurityLog:
    """Log de eventos com encadeamento de hash (tamper-evident)."""

    def __init__(self, path):
        self.path = path
        self._last_hash = self._load_last_hash()

    def _load_last_hash(self):
        if not self.path.exists():
            return GENESIS_HASH
        last = GENESIS_HASH
        with self.path.open("r") as f:
            for line in f:
                last = json.loads(line)["entry_hash"]
        return last

    def append(self, event, detail=""):
        ts = time.time()
        payload = f"{self._last_hash}|{ts}|{event}|{detail}"
        entry_hash = hashlib.sha256(payload.encode()).hexdigest()
        record = {
            "ts": ts,
            "event": event,
            "detail": detail,
            "prev_hash": self._last_hash,
            "entry_hash": entry_hash,
        }
        with self.path.open("a") as f:
            f.write(json.dumps(record) + "\n")
        self._last_hash = entry_hash

    def verify_chain(self):
        """Reprocessa o arquivo e confirma que nenhum registro foi alterado."""
        expected_prev = GENESIS_HASH
        if not self.path.exists():
            return True
        with self.path.open("r") as f:
            for line in f:
                record = json.loads(line)
                if record["prev_hash"] != expected_prev:
                    return False
                payload = f"{record['prev_hash']}|{record['ts']}|{record['event']}|{record['detail']}"
                if hashlib.sha256(payload.encode()).hexdigest() != record["entry_hash"]:
                    return False
                expected_prev = record["entry_hash"]
        return True


class SecureElectronicLock:
    def __init__(self):
        self.keypad = Keypad()
        self.lcd = LCD1602(LCD_I2C_BUS, LCD_I2C_ADDR, cols=LCD_COLS, rows=LCD_ROWS)
        self.buzzer = NonBlockingBuzzer(BUZZER_PIN)
        self.sensor = DistanceSensor(echo=SENSOR_ECHO_PIN, trigger=SENSOR_TRIG_PIN, max_distance=4.0)
        self.log = SecurityLog(LOG_PATH)

        self.state = STATE_LOCKED
        self.buffer = ""
        self.failed_attempts = 0
        self.lockout_count = 0        # usado no backoff exponencial
        self.state_until = None
        self._next_sensor_poll = 0.0
        self._physical_locked = True
        self._pending_relock = False   # UNLOCKED expirou, aguardando sensor confirmar porta fechada
        self._last_distance = None
        self._static_streak = 0

        if not self.log.verify_chain():
            # Cadeia de log corrompida = evidência de adulteração pós-fato;
            # o sistema deve pelo menos alertar, mesmo continuando a operar.
            print("[SECURITY] AVISO: cadeia de log inconsistente (possível adulteração).")

        self.log.append("boot")
        self._render()

    # -- sensor -------------------------------------------------------------
    def _poll_sensor(self, now):
        if now < self._next_sensor_poll:
            return
        self._next_sensor_poll = now + SENSOR_POLL_INTERVAL_S

        distance_cm = self.sensor.distance * 100
        suspicious = self._check_sensor_plausibility(distance_cm)
        self._last_distance = distance_cm

        self._physical_locked = distance_cm <= SENSOR_LOCKED_MAX_CM

        if suspicious:
            self.log.append("sensor_suspect", f"distancia={distance_cm:.2f}cm")
            self._enter_alert("Sensor suspeito")
            return

        # Auto-relock só arma o alarme quando a porta realmente fechou:
        # o timeout do UNLOCKED apenas marca a intenção de trancar
        # (_pending_relock); a transição para LOCKED espera o sensor.
        if self._pending_relock and self._physical_locked:
            self._pending_relock = False
            self.state = STATE_LOCKED
            self._render()
            return

        if self.state == STATE_LOCKED and not self._physical_locked:
            self.log.append("forced_open", f"distancia={distance_cm:.2f}cm")
            self._enter_alert("Violacao fisica")

    def _check_sensor_plausibility(self, distance_cm):
        low, high = SENSOR_VALID_RANGE_CM
        if not (low <= distance_cm <= high):
            return True  # fora da faixa física do HC-SR04 -> sinal manipulado

        if self._last_distance is not None and abs(distance_cm - self._last_distance) < 0.05:
            self._static_streak += 1
        else:
            self._static_streak = 0

        # Leitura perfeitamente estática por tempo demais é consistente com
        # o "Passo 2" do vetor de ataque do PDF (jumper/ímã fixando o pino).
        return self._static_streak >= SENSOR_STATIC_STREAK_LIMIT

    def _enter_alert(self, reason):
        self.state = STATE_ALERT
        self.buffer = ""
        self.buzzer.beep(BEEP_FAIL_S)
        self._render(alert_reason=reason)

    # -- teclado --------------------------------------------------------------
    def _handle_key(self, key, now):
        if self.state == STATE_COOLDOWN:
            return

        if self.state == STATE_ALERT:
            if key == SUBMIT_KEY and self._physical_locked:
                self.state = STATE_LOCKED
                self.log.append("alert_cleared")
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
        if verify_password(self.buffer):
            self.failed_attempts = 0
            self.lockout_count = 0
            self.state = STATE_UNLOCKED
            self.state_until = now + AUTO_RELOCK_S
            self.buzzer.beep(BEEP_SUCCESS_S)
            self.log.append("access_granted")
        else:
            self.failed_attempts += 1
            self.buzzer.beep(BEEP_FAIL_S)
            self.log.append("access_denied", f"tentativa={self.failed_attempts}")
            if self.failed_attempts >= MAX_FAILED_ATTEMPTS:
                cooldown = COOLDOWN_S * (2 ** self.lockout_count)
                self.lockout_count += 1
                self.state = STATE_COOLDOWN
                self.state_until = now + cooldown
                self.log.append("lockout", f"duracao={cooldown:.0f}s")
            else:
                self.state = STATE_LOCKED
        self._pending_relock = False
        self.buffer = ""

    # -- transições dependentes de tempo -----------------------------------
    def _update_timed_state(self, now):
        if self.state_until is None or now < self.state_until:
            return
        self.state_until = None
        if self.state == STATE_UNLOCKED:
            if self._physical_locked:
                self.state = STATE_LOCKED
            else:
                self._pending_relock = True    # espera o sensor confirmar antes de rearmar
        elif self.state == STATE_COOLDOWN:
            self.failed_attempts = 0
            self.state = STATE_LOCKED
        self._render()

    # -- LCD ------------------------------------------------------------------
    def _render(self, alert_reason=None):
        if self.state == STATE_LOCKED:
            masked = "*" * len(self.buffer)
            self.lcd.write_status("STATUS: Trancada", masked or "Digite a senha")
        elif self.state == STATE_UNLOCKED:
            if self._pending_relock:
                self.lcd.write_status("Feche a porta", "para travar")
            else:
                self.lcd.write_status("STATUS: Aberto", "Bem-vindo!")
        elif self.state == STATE_COOLDOWN:
            remaining = max(0, int((self.state_until or 0) - time.monotonic()))
            self.lcd.write_status("Bloqueado", f"Aguarde {remaining}s")
        elif self.state == STATE_ALERT:
            self.lcd.write_status("ALERTA!", (alert_reason or "Violacao")[:16])

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

                if self.state == STATE_COOLDOWN:
                    self._render()

                time.sleep(MAIN_LOOP_INTERVAL_S)
        except KeyboardInterrupt:
            print("Interrompido pelo usuario.")
        finally:
            self.close()

    def close(self):
        self.log.append("shutdown")
        self.keypad.close()
        self.buzzer.close()
        self.sensor.close()
        self.lcd.close()


if __name__ == "__main__":
    SecureElectronicLock().run()
