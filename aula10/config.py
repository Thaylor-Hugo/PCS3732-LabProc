"""Mapeamento de pinos GPIO (BCM) e parâmetros usados por todos os scripts
do projeto Fechadura Eletrônica (Aula 10 — Freenove FNK0054).

Pinos escolhidos para não conflitar entre si na mesma protoboard. Se a sua
fiação usar outros pinos, ajuste apenas os valores abaixo — nenhum outro
arquivo precisa ser tocado.

Referência de capítulos do kit (docs.freenove.com/projects/fnk0054):
  - Matrix Keypad (4x4)      -> linhas/colunas em GPIO digital
  - I2C LCD1602 (PCF8574)    -> barramento I2C (/dev/i2c-1, SDA=GPIO2, SCL=GPIO3)
  - HC-SR04 (ultrassônico)   -> TRIG (saída) / ECHO (entrada, usar divisor
    resistivo 5V->3.3V no ECHO para não danificar o GPIO)
  - Buzzer ativo             -> GPIO digital
"""

# --- Teclado matricial 4x4 -------------------------------------------------
# Colunas = saídas (uma por vez em nível baixo), linhas = entradas com
# pull-up interno (nível baixo = tecla pressionada). Pinagem confirmada
# funcional em MatrixKeypad.py/Keypad.py (referência freenove).
KEYPAD_ROW_PINS = [16, 20, 21, 26]
KEYPAD_COL_PINS = [19, 13, 6, 5]
KEYPAD_LAYOUT = [
    ["1", "2", "3", "A"],
    ["4", "5", "6", "B"],
    ["7", "8", "9", "C"],
    ["*", "0", "#", "D"],
]

# --- Display LCD 1602 via I2C (backpack PCF8574) ----------------------------
LCD_I2C_BUS = 1        # /dev/i2c-1
LCD_I2C_ADDR = 0x27    # confirmar com `i2cdetect -y 1`
LCD_COLS = 16
LCD_ROWS = 2

# --- Sensor ultrassônico HC-SR04 (monitoramento da tranca) ------------------
SENSOR_TRIG_PIN = 23
SENSOR_ECHO_PIN = 24
SENSOR_LOCKED_MAX_CM = 5.0   # distância <= isso => obstáculo/lingueta presente (TRANCADA)
SENSOR_POLL_INTERVAL_S = 0.25

# --- Buzzer ativo ------------------------------------------------------------
BUZZER_PIN = 25
BEEP_SUCCESS_S = 0.15   # bipe curto
BEEP_FAIL_S = 0.6       # bipe longo
BEEP_ALERT_S = 0.15     # usado em pulsos repetidos no alarme

# --- Regras de senha e segurança --------------------------------------------
PASSWORD_MIN_LEN = 4
PASSWORD_MAX_LEN = 6
DEFAULT_PASSWORD = "1234"     # versão normal: comparação em texto plano

SUBMIT_KEY = "#"     # confirma a senha digitada
CLEAR_KEY = "*"      # apaga um dígito (backspace)

MAX_FAILED_ATTEMPTS = 3
COOLDOWN_S = 10.0             # bloqueio temporário após MAX_FAILED_ATTEMPTS (RNF1)
AUTO_RELOCK_S = 5.0           # tempo em "Aberto" antes de retrancar automaticamente

MAIN_LOOP_INTERVAL_S = 0.02   # 20ms: garante teclado responsivo e LCD < 200ms (RF2)
KEYPAD_DEBOUNCE_S = 0.05      # 50ms, conforme funil de depuração do PDF

