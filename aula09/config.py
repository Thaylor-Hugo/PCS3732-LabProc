"""Mapeamento de pinos GPIO (BCM) usado por todos os scripts do projeto Metrônomo RPi3.

Ver diagrama "Arquitetura Física e Roteamento de Interfaces" no PDF de referência.
"""

# Atuadores
LED_PIN = 18       # Sinal PWM -> resistor 330R -> LED -> GND
SERVO_PIN = 13     # Sinal PWM 50Hz -> Servomotor SG90
BUZZER_PIN = 23    # Sinal digital -> Buzzer -> GND

# Botões (desafio) - pull-down interno, borda de subida no clique
BUTTON_UP_PIN = 5     # Aumenta o BPM
BUTTON_DOWN_PIN = 6   # Diminui o BPM

# Parâmetros do servo (largura de pulso em % de duty cycle a 50Hz / período 20ms)
SERVO_FREQ_HZ = 50
SERVO_DC_MIN = 2.5    # ~0.5ms -> 0 graus
SERVO_DC_MAX = 12.5   # ~2.5ms -> 180 graus

# Parâmetros do metrônomo
BPM_DEFAULT = 60       # 60 BPM = 1 batida por segundo (RF01)
BPM_MIN = 30
BPM_MAX = 240
BPM_STEP = 5

BUTTON_BOUNCETIME_MS = 200  # Debounce por software (RNF01)
