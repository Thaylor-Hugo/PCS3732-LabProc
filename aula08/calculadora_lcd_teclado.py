#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
================================================================================
 CALCULADORA BINARIA STANDALONE  -  Raspberry Pi 3 (ARM Cortex-A53)
 PCS3732 - Laboratorio de Processadores  |  Aula 08 - "Arquiteturas em Duelo"
--------------------------------------------------------------------------------
 Desafio Standalone Avancado (LAB-ARCH-008): desacoplar do PC.
   Entrada  : Teclado matricial fisico 4x4  -> multiplexacao GPIO (linha/coluna)
   Saida    : Display LCD 16x2  -> barramento I2C (SDA/SCL, PCF8574 + HD44780)
   Core     : SoC ARM (Raspberry Pi 3), acesso local direto via GPIO.

 KIT: Freenove Ultimate Starter Kit (FNK0054). Pinos e enderecos abaixo seguem
      os tutoriais oficiais do kit (Cap. 21 Matrix Keypad e Cap. 13 I2C-LCD1602).

 Reimplementa, em Python, a MESMA calculadora do arquivo de referencia main.cpp:
   - Numeros binarios de N bits em complemento de dois (N configuravel).
   - Operacoes: [A]dd, [B]sub, [C]mult, [D]div (tratamento de excecao)
                e [*]fatorial (monitoramento de overflow).
   - Deteccao de overflow e benchmark de tempo do core ARM.

--------------------------------------------------------------------------------
 REQUISITOS (no Raspberry Pi):
     sudo apt install python3-rpi.gpio i2c-tools
     pip3 install RPLCD smbus2
     # habilite o I2C:  sudo raspi-config -> Interface Options -> I2C
     # descubra o endereco do LCD:  i2cdetect -y 1   (tipicamente 0x27 ou 0x3F)

 LIGACAO DO LCD (I2C):
     VCC -> 5V   |   GND -> GND   |   SDA -> GPIO2 (pino 3)   |   SCL -> GPIO3 (pino 5)

 LIGACAO DO TECLADO MATRICIAL 4x4 (BCM, ver KEYPAD_ROWS / KEYPAD_COLS abaixo):
     Linhas (rows) = saidas  |  Colunas (cols) = entradas com pull-up interno.

 MAPA DE TECLAS (significado depende do estado / tela atual):
     +---+---+---+---+
     | 1 | 2 | 3 | A |      digitos 0-9 : entrada numerica (bits) e binaria (0/1)
     | 4 | 5 | 6 | B |      A/B/C/D     : escolha da operacao (add/sub/mult/div)
     | 7 | 8 | 9 | C |      *           : apagar (nas telas de entrada)
     | * | 0 | # | D |                    ou FATORIAL (na tela de operacao)
     +---+---+---+---+      #           : confirmar / avancar
================================================================================
"""

"""
2. Preparar o Raspberry Pi (uma vez só)

# habilitar o I2C
sudo raspi-config      # Interface Options -> I2C -> Yes

# dependências
sudo apt update
sudo apt install -y python3-rpi.gpio i2c-tools python3-pip
pip3 install RPLCD smbus2
"""

import time

# ------------------------------------------------------------------------------
# Dependencias de hardware. Importadas de forma protegida para que a logica
# (nucleo aritmetico) possa ser testada mesmo fora do Raspberry Pi.
# ------------------------------------------------------------------------------
try:
    import RPi.GPIO as GPIO
    from RPLCD.i2c import CharLCD
    HARDWARE_DISPONIVEL = True
except ImportError:  # pragma: no cover - ambiente sem hardware
    HARDWARE_DISPONIVEL = False


# ==============================================================================
# CONFIGURACAO DE HARDWARE
# ==============================================================================
# --- LCD I2C (FNK0054 usa PCF8574T=0x27 ou PCF8574AT=0x3F; ver i2cdetect -y 1) ---
LCD_ENDERECO_I2C = 0x27      # troque para 0x3F se o chip for PCF8574AT
LCD_EXPANSOR = "PCF8574"     # backpack do LCD1602 que acompanha o kit
LCD_COLUNAS = 16
LCD_LINHAS = 2

# --- Teclado matricial 4x4 (numeracao BCM, pinos do tutorial FNK0054 Cap. 21) ---
KEYPAD_ROWS = [16, 20, 21, 26]   # linhas -> saidas
KEYPAD_COLS = [19, 13, 6, 5]     # colunas -> entradas (pull-up)
KEYPAD_LAYOUT = [
    ['1', '2', '3', 'A'],
    ['4', '5', '6', 'B'],
    ['7', '8', '9', 'C'],
    ['*', '0', '#', 'D'],
]

# Faixa de bits aceita (fiel ao main.cpp: "2 a 16, ou mais para escalabilidade").
BITS_MIN = 2
BITS_MAX = 64

# Mapeia teclas de letra (e '*') para as operacoes da ULA.
TECLA_PARA_OP = {
    'A': 'add',
    'B': 'sub',
    'C': 'mult',
    'D': 'div',
    '*': 'fat',
}
SIMBOLO_OP = {'add': '+', 'sub': '-', 'mult': '*', 'div': '/', 'fat': '!'}


# ==============================================================================
# NUCLEO ARITMETICO  (porte direto de main.cpp)
# ==============================================================================
def binary_to_decimal(bin_str, bits):
    """Converte string binaria em inteiro interpretando o complemento de dois."""
    unsigned = int(bin_str, 2)
    sign_mask = 1 << (bits - 1)
    if unsigned & sign_mask:
        return unsigned - (1 << bits)
    return unsigned


def decimal_to_binary_string(value, bits):
    """Converte o decimal de volta para string binaria formatada com N bits."""
    mask = (1 << bits) - 1
    masked = value & mask            # em Python, & com mascara ja emula o wrap
    return format(masked, '0{}b'.format(bits))


def _trunc_div(a, b):
    """Divisao inteira truncando em direcao ao zero (como o operador / de C++)."""
    q = abs(a) // abs(b)
    if (a < 0) != (b < 0):
        q = -q
    return q


def executar_calculo(bin_a, bin_b, op, bits):
    """
    Executa a operacao e devolve um dicionario com:
        resultado (int mascarado), overflow (bool), tempo_us (float).
    Mesma logica de CalcResult/executarCalculo em main.cpp.
    """
    mask = (1 << bits) - 1
    min_signed = -(1 << (bits - 1))
    max_signed = (1 << (bits - 1)) - 1

    # Valores com sinal (complemento de dois) dos operandos.
    s_a = binary_to_decimal(bin_a, bits)
    s_b = binary_to_decimal(bin_b, bits) if op != 'fat' else 0

    overflow = False
    resultado = 0

    # --- Inicio do benchmark de alta precisao (nucleo ARM nativo) ---
    inicio = time.perf_counter_ns()

    if op == 'add':
        full = s_a + s_b
        overflow = full < min_signed or full > max_signed
        resultado = full & mask

    elif op == 'sub':
        full = s_a - s_b
        overflow = full < min_signed or full > max_signed
        resultado = full & mask

    elif op == 'mult':
        full = s_a * s_b
        overflow = full < min_signed or full > max_signed
        resultado = full & mask

    elif op == 'div':
        if s_b == 0:
            overflow = True          # Tratamento de excecao: divisao por zero
            resultado = 0
        else:
            full = _trunc_div(s_a, s_b)
            overflow = full < min_signed or full > max_signed
            resultado = full & mask

    elif op == 'fat':
        if s_a < 0:
            overflow = True
            resultado = 0
        else:
            acc = 1
            for i in range(1, s_a + 1):
                acc *= i
                if acc < min_signed or acc > max_signed:
                    overflow = True
            resultado = acc & mask

    fim = time.perf_counter_ns()
    # --- Fim do benchmark ---

    return {
        'resultado': resultado,
        'overflow': overflow,
        'tempo_us': (fim - inicio) / 1000.0,
    }


# ==============================================================================
# DRIVER DO TECLADO MATRICIAL (multiplexacao GPIO linha/coluna)
# ==============================================================================
class TecladoMatricial:
    """Varredura de um teclado 4x4: linhas como saidas, colunas com pull-up."""

    def __init__(self, rows, cols, layout, debounce_s=0.03):
        self.rows = rows
        self.cols = cols
        self.layout = layout
        self.debounce_s = debounce_s

        for r in self.rows:
            GPIO.setup(r, GPIO.OUT, initial=GPIO.HIGH)
        for c in self.cols:
            GPIO.setup(c, GPIO.IN, pull_up_down=GPIO.PUD_UP)

    def _tecla_pressionada(self):
        """Retorna a tecla atualmente pressionada, ou None."""
        for i, r in enumerate(self.rows):
            GPIO.output(r, GPIO.LOW)            # ativa uma linha por vez
            for j, c in enumerate(self.cols):
                if GPIO.input(c) == GPIO.LOW:   # coluna puxada para baixo -> tecla
                    GPIO.output(r, GPIO.HIGH)
                    return self.layout[i][j]
            GPIO.output(r, GPIO.HIGH)
        return None

    def ler_tecla(self):
        """Bloqueia ate uma tecla ser pressionada e solta (com debounce)."""
        while True:
            tecla = self._tecla_pressionada()
            if tecla is not None:
                time.sleep(self.debounce_s)                 # debounce
                if self._tecla_pressionada() == tecla:
                    while self._tecla_pressionada() is not None:
                        time.sleep(0.01)                    # espera soltar
                    return tecla
            time.sleep(0.01)


# ==============================================================================
# CAMADA DE APRESENTACAO (LCD 16x2 via I2C)
# ==============================================================================
class Tela:
    """Pequena fachada sobre o RPLCD para escrever duas linhas de 16 colunas."""

    def __init__(self, lcd):
        self.lcd = lcd

    def mostrar(self, linha0="", linha1=""):
        self.lcd.clear()
        self.lcd.cursor_pos = (0, 0)
        self.lcd.write_string(linha0[:LCD_COLUNAS])
        self.lcd.cursor_pos = (1, 0)
        self.lcd.write_string(linha1[:LCD_COLUNAS])


# ==============================================================================
# MAQUINA DE ESTADOS DA CALCULADORA
# ==============================================================================
class Calculadora:
    def __init__(self, tela, teclado):
        self.tela = tela
        self.teclado = teclado

    # ------- Rotinas de entrada -------
    def ler_inteiro(self, titulo, minimo, maximo):
        """Le um numero decimal via teclado. * apaga, # confirma."""
        buffer = ""
        while True:
            valor = buffer if buffer else "_"
            self.tela.mostrar(titulo, "{}  (*=del #=ok)".format(valor)[:LCD_COLUNAS])
            k = self.teclado.ler_tecla()
            if k.isdigit():
                if len(buffer) < len(str(maximo)):
                    buffer += k
            elif k == '*':
                buffer = buffer[:-1]
            elif k == '#' and buffer:
                n = int(buffer)
                if minimo <= n <= maximo:
                    return n
                # fora da faixa: avisa e reinicia o buffer
                self.tela.mostrar("Faixa: {}..{}".format(minimo, maximo), "Tente de novo")
                time.sleep(1.2)
                buffer = ""

    def ler_binario(self, titulo, bits):
        """Le uma string binaria de ate 'bits' digitos. * apaga, # confirma."""
        buffer = ""
        while True:
            mostrado = buffer.rjust(bits, '.') if buffer else '.' * bits
            self.tela.mostrar("{} ({}b)".format(titulo, bits)[:LCD_COLUNAS], mostrado)
            k = self.teclado.ler_tecla()
            if k in ('0', '1'):
                if len(buffer) < bits:
                    buffer += k
            elif k == '*':
                buffer = buffer[:-1]
            elif k == '#' and buffer:
                return buffer.rjust(bits, '0')   # completa com zeros a esquerda

    def escolher_operacao(self):
        """Mostra o menu de operacoes e retorna o codigo da operacao."""
        while True:
            self.tela.mostrar("OP: A+  B-  C*", "D/   *=! (fat)")
            k = self.teclado.ler_tecla()
            if k in TECLA_PARA_OP:
                return TECLA_PARA_OP[k]

    # ------- Saida de resultado -------
    def mostrar_resultado(self, r, bits):
        bin_str = decimal_to_binary_string(r['resultado'], bits)
        dec = binary_to_decimal(bin_str, bits)
        ov = "SIM" if r['overflow'] else "NAO"

        paginas = [
            ("Bin: " + bin_str, "Dec: {}".format(dec)),
            ("Overflow: " + ov, "t: {:.1f} us".format(r['tempo_us'])),
        ]
        idx = 0
        while True:
            l0, l1 = paginas[idx]
            # rodape indicando a navegacao
            self.tela.mostrar(l0, l1)
            k = self.teclado.ler_tecla()
            if k == '#':
                idx = (idx + 1) % len(paginas)
            elif k == '*':
                return   # reinicia o ciclo

    # ------- Laco principal -------
    def executar(self):
        self.tela.mostrar("Calc. Binaria", "RPi3 - ARM A53")
        time.sleep(1.5)

        while True:
            bits = self.ler_inteiro("Num. de bits:", BITS_MIN, BITS_MAX)
            bin_a = self.ler_binario("Operando A", bits)
            op = self.escolher_operacao()

            if op == 'fat':
                bin_b = "0" * bits
            else:
                bin_b = self.ler_binario("Operando B", bits)

            # Eco da expressao antes de calcular.
            self.tela.mostrar(
                "{} {} {}".format(bin_a, SIMBOLO_OP[op], "" if op == 'fat' else bin_b)[:LCD_COLUNAS],
                "Calculando...",
            )
            time.sleep(0.4)

            resultado = executar_calculo(bin_a, bin_b, op, bits)
            self.mostrar_resultado(resultado, bits)
            # apos '*' na tela de resultado, o laco recomeca


# ==============================================================================
# BOOTSTRAP
# ==============================================================================
def main():
    if not HARDWARE_DISPONIVEL:
        raise SystemExit(
            "Bibliotecas de hardware nao encontradas.\n"
            "Instale no Raspberry Pi:\n"
            "    sudo apt install python3-rpi.gpio i2c-tools\n"
            "    pip3 install RPLCD smbus2\n"
            "e habilite o I2C via raspi-config."
        )

    GPIO.setmode(GPIO.BCM)
    GPIO.setwarnings(False)

    lcd = CharLCD(
        i2c_expander=LCD_EXPANSOR,
        address=LCD_ENDERECO_I2C,
        port=1,
        cols=LCD_COLUNAS,
        rows=LCD_LINHAS,
        dotsize=8,
    )

    try:
        tela = Tela(lcd)
        teclado = TecladoMatricial(KEYPAD_ROWS, KEYPAD_COLS, KEYPAD_LAYOUT)
        Calculadora(tela, teclado).executar()
    except KeyboardInterrupt:
        pass
    finally:
        lcd.clear()
        lcd.write_string("Encerrado.")
        GPIO.cleanup()


if __name__ == "__main__":
    main()
