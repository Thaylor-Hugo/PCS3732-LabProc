"""Mapeamento de pinos GPIO (BCM) usado por todos os scripts do projeto Metrônomo RPi3.

Pinos escolhidos para coincidir com os capítulos correspondentes da
documentação oficial do kit Freenove FNK0054 (gpiozero), evitando
remontar a fiação da protoboard:
  https://docs.freenove.com/projects/fnk0054/en/latest/fnk0054/c%26py.html
  - Cap. 4 (Analog & PWM)      -> LED em GPIO17
  - Cap. 13 (Servo)            -> Servo em GPIO18
  - Cap. 6 (Buzzer, doorbell)  -> Buzzer ativo em GPIO12
  - Cap. 3 (Buttons & LEDs)    -> Botões em GPIO20/GPIO21

Se a sua fiação usar outros pinos, ajuste os valores abaixo.
"""

# Atuadores
LED_PIN = 17       # PWMLED -> resistor 330R -> LED -> GND
SERVO_PIN = 18     # AngularServo -> Servomotor SG90
BUZZER_PIN = 12    # Buzzer (ativo) -> GND

# Botões (desafio) - debounce nativo do gpiozero (Button bounce_time)
BUTTON_UP_PIN = 20     # Aumenta o BPM
BUTTON_DOWN_PIN = 21   # Diminui o BPM

# Parâmetros do servo SG90 (largura de pulso em segundos, igual ao
# exemplo Sweep.py do capítulo 13 da Freenove)
SERVO_MIN_PULSE_S = 0.5 / 1000
SERVO_MAX_PULSE_S = 2.5 / 1000

# Parâmetros do metrônomo
BPM_DEFAULT = 60       # 60 BPM = 1 batida por segundo (RF01)
BPM_MIN = 30
BPM_MAX = 240
BPM_STEP = 5

BUTTON_BOUNCE_TIME_S = 0.2  # Debounce por software (RNF01)
