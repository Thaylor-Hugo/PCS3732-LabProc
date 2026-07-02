#include <iostream>
#include <string>
#include <cmath>
#include <chrono>
#include <cstdlib>
#include <wiringPi.h>
#include <wiringPiI2C.h>
#include <unistd.h>

// ==========================================
// CONFIGURAÇÕES DO DISPLAY LCD I2C
// ==========================================
#define I2C_ADDR 0x27 // Endereço comum do PCF8574. Tente 0x3F se não funcionar.
#define LCD_CHR  1 // Modo envio de caractere
#define LCD_CMD  0 // Modo envio de comando
#define LINE1  0x80 // Endereço da 1a linha
#define LINE2  0xC0 // Endereço da 2a linha
#define LCD_BACKLIGHT   0x08  // Bit 3 do PCF8574 (Ligado)
#define ENABLE  0b00000100 // Bit 2 do PCF8574

int lcd_fd;

void lcd_toggle_enable(int bits) {
    delayMicroseconds(500);
    wiringPiI2CWrite(lcd_fd, (bits | ENABLE));
    delayMicroseconds(500);
    wiringPiI2CWrite(lcd_fd, (bits & ~ENABLE));
    delayMicroseconds(500);
}

void lcd_byte(int bits, int mode) {
    int bits_high = mode | (bits & 0xF0) | LCD_BACKLIGHT;
    int bits_low = mode | ((bits << 4) & 0xF0) | LCD_BACKLIGHT;

    wiringPiI2CWrite(lcd_fd, bits_high);
    lcd_toggle_enable(bits_high);

    wiringPiI2CWrite(lcd_fd, bits_low);
    lcd_toggle_enable(bits_low);
}

void lcd_init() {
    lcd_fd = wiringPiI2CSetup(I2C_ADDR);
    if (lcd_fd == -1) {
        std::cerr << "Erro ao inicializar o I2C do display LCD.\n";
        exit(1);
    }
    lcd_byte(0x33, LCD_CMD);
    lcd_byte(0x32, LCD_CMD);
    lcd_byte(0x06, LCD_CMD);
    lcd_byte(0x0C, LCD_CMD);
    lcd_byte(0x28, LCD_CMD);
    lcd_byte(0x01, LCD_CMD);
    delayMicroseconds(500);
}

void lcd_clear() {
    lcd_byte(0x01, LCD_CMD);
    delay(2);
}

void lcd_loc(int line) {
    lcd_byte(line, LCD_CMD);
}

void lcd_print(const char *s) {
    while (*s) {
        lcd_byte(*(s++), LCD_CHR);
    }
}

void lcd_print(std::string s) {
    lcd_print(s.c_str());
}

// ==========================================
// CONFIGURAÇÕES DO TECLADO MATRICIAL 4X4
// ==========================================
// Usando numeração BCM (GPIO do Raspberry) em vez de WiringPi pinos
const int ROW[4] = {16, 20, 21, 26}; // Linhas do teclado
const int COL[4] = {19, 13, 6, 5}; // Colunas do teclado

char keys[4][4] = {
  {'1','2','3','A'},
  {'4','5','6','B'},
  {'7','8','9','C'},
  {'*','0','#','D'}
};

void keypad_init() {
    // Força a ativação dos resistores de pull-up internos via sistema.
    // Isso corrige um bug comum do wiringPi em Raspberry Pi 4/5 onde o pullUpDnControl falha
    // e deixa os pinos flutuando (floating), causando esse comportamento de "travar".
    system("raspi-gpio set 19,13,6,5 pu > /dev/null 2>&1 || pinctrl set 19,13,6,5 pu > /dev/null 2>&1");

    for (int i=0; i<4; i++) {
        pinMode(ROW[i], OUTPUT);
        digitalWrite(ROW[i], HIGH);
        pinMode(COL[i], INPUT);
        pullUpDnControl(COL[i], PUD_UP);
    }
}

char keypad_scan() {
    for (int r=0; r<4; r++) {
        digitalWrite(ROW[r], LOW);
        for (int c=0; c<4; c++) {
            if (digitalRead(COL[c]) == LOW) {
                delay(20); // Debounce
                if (digitalRead(COL[c]) == LOW) {
                    while (digitalRead(COL[c]) == LOW) { delay(10); } // Aguarda soltar
                    digitalWrite(ROW[r], HIGH);
                    return keys[r][c];
                }
            }
        }
        digitalWrite(ROW[r], HIGH);
    }
    return '\0';
}

// ==========================================
// LÓGICA DA CALCULADORA (Adaptada do main.cpp)
// ==========================================
struct CalcResult {
    int resultado;
    bool overflow;
    long long timeSpentMicros;
};

int binaryToDecimal(const std::string& bin, int bits) {
    int unsignedValue = std::stol(bin, nullptr, 2);
    int signMask = 1 << (bits - 1);
    if ((unsignedValue & signMask) != 0) {
        return unsignedValue - (1 << bits);
    }
    return unsignedValue;
}

std::string decimalToBinaryString(int value, int bits) {
    std::string s = "";
    int mask = (1 << bits) - 1;
    int maskedVal = value & mask;
    for (int i = bits - 1; i >= 0; i--) {
        s += ((maskedVal >> i) & 1) ? "1" : "0";
    }
    return s;
}

CalcResult executarCalculo(std::string binA, std::string binB, std::string op, int bits) {
    CalcResult res;
    res.overflow = false;
    res.resultado = 0;

    int mask = (bits >= 32) ? -1 : ((1 << bits) - 1);
    int signMask = 1 << (bits - 1);
    int minSigned = -(1 << (bits - 1));
    int maxSigned = (1 << (bits - 1)) - 1;

    int valA = binaryToDecimal(binA, bits) & mask;
    int valB = (op != "fat") ? (binaryToDecimal(binB, bits) & mask) : 0;

    int sA = (valA & signMask) ? (valA - (1 << bits)) : valA;
    int sB = (valB & signMask) ? (valB - (1 << bits)) : valB;

    long long full = 0;

    auto start = std::chrono::high_resolution_clock::now();

    if (op == "add") {
        full = (long long)sA + sB;
        if (full < minSigned || full > maxSigned) res.overflow = true;
        res.resultado = ((int)full) & mask;
    } 
    else if (op == "sub") {
        full = (long long)sA - sB;
        if (full < minSigned || full > maxSigned) res.overflow = true;
        res.resultado = ((int)full) & mask;
    } 
    else if (op == "mult") {
        full = (long long)sA * sB;
        if (full < minSigned || full > maxSigned) res.overflow = true;
        res.resultado = ((int)full) & mask;
    } 
    else if (op == "div") {
        if (sB == 0) {
            res.overflow = true;
            res.resultado = 0;
        } else {
            full = (long long)sA / sB;
            if (full < minSigned || full > maxSigned) res.overflow = true;
            res.resultado = ((int)full) & mask;
        }
    } 
    else if (op == "fat") {
        if (sA < 0) {
            res.overflow = true;
            res.resultado = 0;
        } else {
            long long acc = 1;
            for (int i = 1; i <= sA; i++) {
                acc *= i;
                if (acc < minSigned || acc > maxSigned) res.overflow = true;
            }
            res.resultado = ((int)acc) & mask;
        }
    }

    auto end = std::chrono::high_resolution_clock::now();
    res.timeSpentMicros = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();

    return res;
}

// ==========================================
// MÁQUINA DE ESTADOS DA INTERFACE (FSM)
// ==========================================
enum State { SET_BITS, GET_A, GET_OP, GET_B, SHOW_RESULT };

int main() {
    // Usando wiringPiSetupGpio() para que a numeração BCM seja reconhecida corretamente
    if (wiringPiSetupGpio() == -1) {
        std::cerr << "Erro ao inicializar wiringPi\n";
        return 1;
    }
    
    lcd_init();
    keypad_init();

    State currentState = SET_BITS;
    int bits = 0;
    std::string binA = "";
    std::string binB = "";
    std::string op = "";
    std::string currentInput = "";
    
    lcd_clear();
    lcd_loc(LINE1);
    lcd_print("Bits (2-16):");

    while(true) {
        char k = keypad_scan();
        if (k != '\0') {
            if (currentState == SET_BITS) {
                if (k >= '0' && k <= '9') {
                    if (currentInput.length() < 2) currentInput += k; // máx 2 digitos para bits
                    lcd_loc(LINE2);
                    lcd_print(currentInput + "  ");
                } else if (k == 'B') { 
                    if (currentInput.length() > 0) currentInput.pop_back();
                    lcd_loc(LINE2);
                    lcd_print(currentInput + "  ");
                } else if (k == 'A') { // ENTER
                    if (currentInput.length() > 0) {
                        bits = std::stoi(currentInput);
                        if (bits >= 2 && bits <= 16) { 
                            currentState = GET_A;
                            currentInput = "";
                            lcd_clear();
                            lcd_loc(LINE1);
                            lcd_print("Op A (Bin):");
                        } else {
                            lcd_clear();
                            lcd_loc(LINE1);
                            lcd_print("Min 2, Max 16!");
                            delay(1500);
                            lcd_clear();
                            lcd_loc(LINE1);
                            lcd_print("Bits (2-16):");
                            currentInput = "";
                        }
                    }
                }
            } 
            else if (currentState == GET_A) {
                if (k == '0' || k == '1') {
                    if (currentInput.length() < (size_t)bits) currentInput += k;
                    lcd_loc(LINE2);
                    lcd_print(currentInput + " ");
                } else if (k == 'B') {
                    if (currentInput.length() > 0) currentInput.pop_back();
                    lcd_loc(LINE2);
                    lcd_print(currentInput + "  ");
                } else if (k == 'A') {
                    if (currentInput.length() > 0) {
                        binA = currentInput;
                        while (binA.length() < (size_t)bits) binA = "0" + binA; // padding
                        currentState = GET_OP;
                        currentInput = "";
                        lcd_clear();
                        lcd_loc(LINE1);
                        lcd_print("1+ 2- 3* 4/ 5!");
                    }
                }
            } 
            else if (currentState == GET_OP) {
                if (k >= '1' && k <= '5') {
                    if (k == '1') op = "add";
                    if (k == '2') op = "sub";
                    if (k == '3') op = "mult";
                    if (k == '4') op = "div";
                    if (k == '5') op = "fat";
                    
                    if (op == "fat") {
                        currentState = SHOW_RESULT;
                    } else {
                        currentState = GET_B;
                        lcd_clear();
                        lcd_loc(LINE1);
                        lcd_print("Op B (Bin):");
                    }
                }
            } 
            else if (currentState == GET_B) {
                if (k == '0' || k == '1') {
                    if (currentInput.length() < (size_t)bits) currentInput += k;
                    lcd_loc(LINE2);
                    lcd_print(currentInput + " ");
                } else if (k == 'B') {
                    if (currentInput.length() > 0) currentInput.pop_back();
                    lcd_loc(LINE2);
                    lcd_print(currentInput + "  ");
                } else if (k == 'A') {
                    if (currentInput.length() > 0) {
                        binB = currentInput;
                        while (binB.length() < (size_t)bits) binB = "0" + binB;
                        currentState = SHOW_RESULT;
                    }
                }
            }

            if (currentState == SHOW_RESULT) {
                CalcResult r = executarCalculo(binA, binB, op, bits);
                
                std::string l1 = decimalToBinaryString(r.resultado, bits);
                std::string l2;
                
                if (r.overflow) {
                    l2 = "OVF T:" + std::to_string(r.timeSpentMicros) + "us";
                } else {
                    l2 = std::to_string(binaryToDecimal(l1, bits)) + " T:" + std::to_string(r.timeSpentMicros) + "us";
                }
                
                // Truncar para 16 chars por causa do display
                if (l1.length() > 16) l1 = l1.substr(0, 16);
                if (l2.length() > 16) l2 = l2.substr(0, 16);

                lcd_clear();
                lcd_loc(LINE1);
                lcd_print(l1.c_str());
                lcd_loc(LINE2);
                lcd_print(l2.c_str());
                
                // Aguarda a tecla A para reiniciar
                while(true) {
                    char k2 = keypad_scan();
                    if (k2 == 'A') {
                        currentState = SET_BITS;
                        currentInput = "";
                        lcd_clear();
                        lcd_loc(LINE1);
                        lcd_print("Bits (2-16):");
                        break;
                    }
                    delay(50);
                }
            }
        }
        delay(50);
    }
    return 0;
}
