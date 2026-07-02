// ============================================================================
//  CALCULADORA BINARIA STANDALONE  -  Raspberry Pi 3 (ARM Cortex-A53)  |  C++
//  PCS3732 - Laboratorio de Processadores | Aula 08 "Arquiteturas em Duelo"
// ----------------------------------------------------------------------------
//  Desafio Standalone Avancado (LAB-ARCH-008): desacoplar do PC.
//    Entrada : Teclado matricial fisico 4x4 -> multiplexacao GPIO (linha/coluna)
//    Saida   : Display LCD 16x2 -> barramento I2C (SDA/SCL, PCF8574 + HD44780)
//    Core    : SoC ARM (Raspberry Pi 3), acesso local direto via GPIO/WiringPi.
//
//  KIT: Freenove Ultimate Starter Kit (FNK0054).
//    Teclado (BCM): linhas {16,20,21,26}, colunas {19,13,6,5} (Cap. 21).
//    LCD I2C: PCF8574T=0x27 (ou PCF8574AT=0x3F). Verifique com "i2cdetect -y 1".
//
//  Reimplementa a MESMA calculadora de main.cpp, agora N-bits configuravel:
//    add / sub / mult / div (tratamento de excecao) e fatorial (monitoramento
//    de overflow), com complemento de dois e benchmark de tempo do core ARM.
//
// ----------------------------------------------------------------------------
//  COMPILAR (no Raspberry Pi, com WiringPi instalado):
//      g++ calculadora_lcd_teclado.cpp -o calc -lwiringPi
//  EXECUTAR (precisa de acesso a GPIO/I2C):
//      sudo ./calc
//
//  MAPA DE TECLAS (significado depende da tela atual):
//      +---+---+---+---+   digitos 0-9 : entrada numerica (bits) e binaria (0/1)
//      | 1 | 2 | 3 | A |   A/B/C/D     : operacao (add/sub/mult/div)
//      | 4 | 5 | 6 | B |   *           : APAGAR (telas de entrada)
//      | 7 | 8 | 9 | C |                 ou FATORIAL (tela de operacao)
//      | * | 0 | # | D |   #           : CONFIRMAR / avancar
//      +---+---+---+---+
// ============================================================================

#include <iostream>
#include <string>
#include <chrono>
#include <cstdlib>
#include <cstdint>
#include <wiringPi.h>
#include <wiringPiI2C.h>

typedef __int128 i128;   // intermediarios largos (mult/fatorial ate 64 bits)

// ============================================================================
//  CONFIGURACAO DE HARDWARE
// ============================================================================
#define I2C_ADDR       0x27          // troque para 0x3F se o chip for PCF8574AT
#define LCD_CHR        1             // envio de dado (caractere)
#define LCD_CMD        0             // envio de comando
#define LCD_BACKLIGHT  0x08
#define LCD_ENABLE     0b00000100

#define LCD_LINE1      0x80          // DDRAM: inicio da linha 1
#define LCD_LINE2      0xC0          // DDRAM: inicio da linha 2
#define LCD_COLS       16

// Teclado matricial 4x4 (numeracao BCM, tutorial FNK0054 Cap. 21)
const int ROWS[4] = {16, 20, 21, 26};   // linhas -> saidas
const int COLS[4] = {19, 13, 6, 5};     // colunas -> entradas (pull-up)
const char KEYS[4][4] = {
    {'1', '2', '3', 'A'},
    {'4', '5', '6', 'B'},
    {'7', '8', '9', 'C'},
    {'*', '0', '#', 'D'},
};

// Faixa de bits aceita (fiel a main.cpp: "2 a 16, ou mais p/ escalabilidade").
const int BITS_MIN = 2;
const int BITS_MAX = 64;

static int lcd_fd = -1;   // file descriptor do barramento I2C do LCD

// ============================================================================
//  DRIVER DE BAIXO NIVEL DO LCD I2C (PCF8574 + HD44780, modo 4 bits)
// ============================================================================
static void lcd_toggle_enable(int bits) {
    delayMicroseconds(500);
    wiringPiI2CWrite(lcd_fd, (bits | LCD_ENABLE));
    delayMicroseconds(500);
    wiringPiI2CWrite(lcd_fd, (bits & ~LCD_ENABLE));
    delayMicroseconds(500);
}

// Envia um byte ao HD44780 em dois nibbles (4 bits) pelo expansor PCF8574.
static void lcd_byte(int bits, int mode) {
    int high = mode | (bits & 0xF0) | LCD_BACKLIGHT;
    int low  = mode | ((bits << 4) & 0xF0) | LCD_BACKLIGHT;
    wiringPiI2CWrite(lcd_fd, high);
    lcd_toggle_enable(high);
    wiringPiI2CWrite(lcd_fd, low);
    lcd_toggle_enable(low);
}

static bool lcd_init() {
    lcd_fd = wiringPiI2CSetup(I2C_ADDR);
    if (lcd_fd == -1) return false;
    lcd_byte(0x33, LCD_CMD);   // inicializacao
    lcd_byte(0x32, LCD_CMD);   // modo 4 bits
    lcd_byte(0x06, LCD_CMD);   // incremento de cursor
    lcd_byte(0x0C, LCD_CMD);   // display ON, cursor OFF
    lcd_byte(0x28, LCD_CMD);   // 4 bits, 2 linhas, fonte 5x8
    lcd_byte(0x01, LCD_CMD);   // limpa display
    delay(5);
    return true;
}

static void lcd_clear() { lcd_byte(0x01, LCD_CMD); delay(2); }

static void lcd_write(int addr, const std::string& s) {
    lcd_byte(addr, LCD_CMD);
    for (int i = 0; i < LCD_COLS && s[i]; i++) lcd_byte(s[i], LCD_CHR);
}

// Escreve as duas linhas de uma vez (trunca em 16 colunas).
static void lcd_show(const std::string& l0, const std::string& l1 = "") {
    lcd_clear();
    lcd_write(LCD_LINE1, l0.substr(0, LCD_COLS));
    lcd_write(LCD_LINE2, l1.substr(0, LCD_COLS));
}

// ============================================================================
//  DRIVER DO TECLADO MATRICIAL (varredura linha/coluna via GPIO)
// ============================================================================
static void keypad_setup() {
    // No Raspberry Pi OS recente o pull-up interno via WiringPi pode nao pegar;
    // reforca por raspi-gpio/pinctrl (ignora silenciosamente se nao existir).
    system("raspi-gpio set 19,13,6,5 pu 2>/dev/null "
           "|| pinctrl set 19,13,6,5 pu 2>/dev/null");
    for (int i = 0; i < 4; i++) {
        pinMode(ROWS[i], OUTPUT);
        digitalWrite(ROWS[i], HIGH);         // linhas inativas em HIGH
        pinMode(COLS[i], INPUT);
        pullUpDnControl(COLS[i], PUD_UP);
    }
}

// Retorna a tecla pressionada (ja com debounce e espera de soltar), ou '\0'.
static char keypad_scan() {
    for (int r = 0; r < 4; r++) {
        digitalWrite(ROWS[r], LOW);          // ativa uma linha
        for (int c = 0; c < 4; c++) {
            if (digitalRead(COLS[c]) == LOW) {
                delay(30);                   // debounce
                if (digitalRead(COLS[c]) == LOW) {
                    char k = KEYS[r][c];
                    while (digitalRead(COLS[c]) == LOW) delay(10);  // espera soltar
                    digitalWrite(ROWS[r], HIGH);
                    return k;
                }
            }
        }
        digitalWrite(ROWS[r], HIGH);
    }
    return '\0';
}

// Bloqueia ate uma tecla ser lida.
static char keypad_wait() {
    for (;;) {
        char k = keypad_scan();
        if (k != '\0') return k;
        delay(15);
    }
}

// ============================================================================
//  NUCLEO ARITMETICO (porte de main.cpp, generalizado para N bits)
// ============================================================================
struct CalcResult {
    long long resultado;    // valor com sinal ja "envelopado" em N bits
    bool overflow;
    long long tempo_us;
};

// String binaria -> decimal com sinal (complemento de dois).
static long long binary_to_decimal(const std::string& bin, int bits) {
    unsigned long long uv = std::stoull(bin, nullptr, 2);
    if (bits >= 64) return (long long)uv;               // padrao ja e o proprio signed
    unsigned long long sign_mask = 1ULL << (bits - 1);
    if (uv & sign_mask) return (long long)uv - (1LL << bits);
    return (long long)uv;
}

// Decimal -> string binaria de N bits.
static std::string decimal_to_binary_string(long long value, int bits) {
    std::string s;
    unsigned long long u = (unsigned long long)value;
    for (int i = bits - 1; i >= 0; i--) s += ((u >> i) & 1ULL) ? '1' : '0';
    return s;
}

// Reinterpreta os N bits baixos de 'full' como inteiro com sinal (envelope).
static long long wrap_to_signed(i128 full, int bits) {
    unsigned long long mask = (bits >= 64) ? ~0ULL : ((1ULL << bits) - 1);
    unsigned long long low = (unsigned long long)(full & (i128)mask);
    if (bits >= 64) return (long long)low;
    unsigned long long sign_mask = 1ULL << (bits - 1);
    if (low & sign_mask) return (long long)low - (1LL << bits);
    return (long long)low;
}

// Divisao inteira truncando em direcao ao zero (como o operador / de C++).
static i128 trunc_div(i128 a, i128 b) {
    return a / b;   // C++ ja trunca em direcao ao zero
}

static std::string i128_to_string(i128 v);  // fwd

static CalcResult executar_calculo(const std::string& binA,
                                   const std::string& binB,
                                   char op, int bits) {
    i128 min_signed = -((i128)1 << (bits - 1));
    i128 max_signed =  ((i128)1 << (bits - 1)) - 1;

    long long sA = binary_to_decimal(binA, bits);
    long long sB = (op != '*') ? binary_to_decimal(binB, bits) : 0;

    bool overflow = false;
    long long resultado = 0;

    auto start = std::chrono::high_resolution_clock::now();

    if (op == 'A') {                    // add
        i128 full = (i128)sA + sB;
        overflow = full < min_signed || full > max_signed;
        resultado = wrap_to_signed(full, bits);
    } else if (op == 'B') {             // sub
        i128 full = (i128)sA - sB;
        overflow = full < min_signed || full > max_signed;
        resultado = wrap_to_signed(full, bits);
    } else if (op == 'C') {             // mult
        i128 full = (i128)sA * sB;
        overflow = full < min_signed || full > max_signed;
        resultado = wrap_to_signed(full, bits);
    } else if (op == 'D') {             // div
        if (sB == 0) {                  // excecao: divisao por zero
            overflow = true;
            resultado = 0;
        } else {
            i128 full = trunc_div(sA, sB);
            overflow = full < min_signed || full > max_signed;
            resultado = wrap_to_signed(full, bits);
        }
    } else if (op == '*') {             // fatorial
        if (sA < 0) {
            overflow = true;
            resultado = 0;
        } else {
            i128 acc = 1;
            for (long long i = 1; i <= sA; i++) {
                acc *= i;
                if (acc < min_signed || acc > max_signed) { overflow = true; break; }
            }
            resultado = wrap_to_signed(acc, bits);
        }
    }

    auto end = std::chrono::high_resolution_clock::now();

    CalcResult r;
    r.resultado = resultado;
    r.overflow = overflow;
    r.tempo_us = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
    return r;
}

// ============================================================================
//  MAQUINA DE ESTADOS / INTERFACE
// ============================================================================
static int ler_inteiro(const std::string& titulo, int minimo, int maximo) {
    std::string buffer;
    size_t maxlen = std::to_string(maximo).size();
    for (;;) {
        std::string val = buffer.empty() ? "_" : buffer;
        lcd_show(titulo, val + "  (*=del #=ok)");
        char k = keypad_wait();
        if (k >= '0' && k <= '9') {
            if (buffer.size() < maxlen) buffer += k;
        } else if (k == '*') {
            if (!buffer.empty()) buffer.pop_back();
        } else if (k == '#' && !buffer.empty()) {
            int n = std::stoi(buffer);
            if (n >= minimo && n <= maximo) return n;
            lcd_show("Faixa: " + std::to_string(minimo) + ".." + std::to_string(maximo),
                     "Tente de novo");
            delay(1200);
            buffer.clear();
        }
    }
}

static std::string ler_binario(const std::string& titulo, int bits) {
    std::string buffer;
    for (;;) {
        std::string mostrado = buffer;
        while ((int)mostrado.size() < bits) mostrado += '.';   // placeholders a direita
        lcd_show(titulo + " (" + std::to_string(bits) + "b)", mostrado);
        char k = keypad_wait();
        if (k == '0' || k == '1') {
            if ((int)buffer.size() < bits) buffer += k;
        } else if (k == '*') {
            if (!buffer.empty()) buffer.pop_back();
        } else if (k == '#' && !buffer.empty()) {
            while ((int)buffer.size() < bits) buffer = "0" + buffer;  // pad a esquerda
            return buffer;
        }
    }
}

static char escolher_operacao() {
    for (;;) {
        lcd_show("OP: A+  B-  C*", "D/   *=! (fat)");
        char k = keypad_wait();
        if (k == 'A' || k == 'B' || k == 'C' || k == 'D' || k == '*') return k;
    }
}

static void mostrar_resultado(const CalcResult& r, int bits) {
    std::string bin = decimal_to_binary_string(r.resultado, bits);
    std::string ov = r.overflow ? "SIM" : "NAO";

    std::string pag0a = "Bin:" + bin;
    std::string pag0b = "Dec:" + std::to_string(r.resultado);
    std::string pag1a = "Overflow: " + ov;
    std::string pag1b = "t: " + std::to_string(r.tempo_us) + " us";

    int idx = 0;
    for (;;) {
        if (idx == 0) lcd_show(pag0a, pag0b);
        else          lcd_show(pag1a, pag1b);
        char k = keypad_wait();
        if (k == '#') idx = (idx + 1) % 2;    // alterna paginas
        else if (k == '*') return;            // reinicia o ciclo
    }
}

// ============================================================================
//  LOOP PRINCIPAL (standalone / baremetal-like sobre Linux)
// ============================================================================
int main() {
    if (wiringPiSetupGpio() == -1) {
        std::cerr << "Erro ao inicializar WiringPi! Execute com 'sudo'." << std::endl;
        return 1;
    }
    if (!lcd_init()) {
        std::cerr << "Erro ao inicializar o LCD I2C (verifique i2cdetect -y 1)." << std::endl;
        return 1;
    }
    keypad_setup();

    lcd_show("Calc. Binaria", "RPi3 - ARM A53");
    delay(1500);

    for (;;) {
        int bits = ler_inteiro("Num. de bits:", BITS_MIN, BITS_MAX);
        std::string binA = ler_binario("Operando A", bits);
        char op = escolher_operacao();

        std::string binB;
        if (op == '*') binB = std::string(bits, '0');
        else           binB = ler_binario("Operando B", bits);

        lcd_show("Calculando...", "");
        delay(300);

        CalcResult r = executar_calculo(binA, binB, op, bits);
        mostrar_resultado(r, bits);
        // '*' na tela de resultado -> recomeca o laco
    }
    return 0;
}

// ---------------------------------------------------------------------------
// (helper mantido para depuracao; nao usado na UI, que exibe apenas o decimal
//  ja envelopado em long long)
static std::string i128_to_string(i128 v) {
    if (v == 0) return "0";
    bool neg = v < 0;
    unsigned __int128 u = neg ? -(unsigned __int128)v : (unsigned __int128)v;
    std::string s;
    while (u) { s += char('0' + (int)(u % 10)); u /= 10; }
    if (neg) s += '-';
    std::string out(s.rbegin(), s.rend());
    return out;
}
