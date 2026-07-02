#include <iostream>
#include <string>
#include <cmath>
#include <chrono>
#include <cstdlib>
#include <wiringPi.h>
#include <wiringPiI2C.h>

// ==========================================
// CONFIGURAÇÃO DO DISPLAY LCD I2C (PCF8574)
// ==========================================
#define I2C_ADDR 0x27 
#define LCD_CHR  1    
#define LCD_CMD  0    
#define LCD_BACKLIGHT 0x08
#define LCD_ENABLE    0b00000100

int lcd_fd; 

void lcd_toggle_enable(int bits) {
    delayMicroseconds(500);
    wiringPiI2CWrite(lcd_fd, (bits | LCD_ENABLE));
    delayMicroseconds(500);
    wiringPiI2CWrite(lcd_fd, (bits & ~LCD_ENABLE));
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
        std::cerr << "Erro ao inicializar o I2C do display LCD." << std::endl;
    }
    lcd_byte(0x33, LCD_CMD); 
    lcd_byte(0x32, LCD_CMD); 
    lcd_byte(0x06, LCD_CMD); 
    lcd_byte(0x0C, LCD_CMD); 
    lcd_byte(0x28, LCD_CMD); 
    lcd_byte(0x01, LCD_CMD); 
    delay(5);
}

void lcd_clear() {
    lcd_byte(0x01, LCD_CMD);
    delay(5);
}

void lcd_loc(int line) {
    lcd_byte(line, LCD_CMD);
}

void lcd_string(const char *s) {
    while (*s) {
        lcd_byte(*(s++), LCD_CHR);
    }
}

// ==========================================
// CONFIGURAÇÃO DO TECLADO MATRICIAL (Freenove)
// ==========================================
const int ROWS[4] = {16, 20, 21, 26}; 
const int COLS[4] = {19, 13, 6, 5}; 

char keys[4][4] = {
  {'1', '2', '3', 'A'}, 
  {'4', '5', '6', 'B'}, 
  {'7', '8', '9', 'C'}, 
  {'*', '0', '#', 'D'}  
};

void setup_keypad() {
    // Força o acionamento do Pull-Up via kernel para evitar flutuação (spams)
    system("raspi-gpio set 19,13,6,5 pu 2>/dev/null || pinctrl set 19,13,6,5 pu 2>/dev/null");

    for (int i = 0; i < 4; i++) {
        pinMode(ROWS[i], OUTPUT);
        digitalWrite(ROWS[i], HIGH); 
        pinMode(COLS[i], INPUT);
        pullUpDnControl(COLS[i], PUD_UP); 
    }
}

char get_key() {
    for (int r = 0; r < 4; r++) {
        digitalWrite(ROWS[r], LOW); 
        
        // -------------------------------------------------------------
        // CORREÇÃO DE SLEW RATE: Espera a voltagem física cair para 0V
        // antes do rápido processador ARM tentar ler a coluna.
        // -------------------------------------------------------------
        delay(2); 

        for (int c = 0; c < 4; c++) {
            if (digitalRead(COLS[c]) == LOW) {
                delay(30); // Debounce mecânico
                
                if (digitalRead(COLS[c]) == LOW) { 
                    char pressedKey = keys[r][c];
                    
                    while(digitalRead(COLS[c]) == LOW) {
                        delay(10);
                    }
                    
                    digitalWrite(ROWS[r], HIGH); 
                    
                    // Espera a voltagem subir novamente antes de sair
                    delay(2); 
                    return pressedKey;
                }
            }
        }
        digitalWrite(ROWS[r], HIGH); 
        
        // Garante que a linha subiu totalmente antes do loop ir para a próxima
        delay(1); 
    }
    return '\0'; 
}

// ==========================================
// LÓGICA DE CÁLCULO E MATEMÁTICA (4 BITS)
// ==========================================
const int BITS = 4;
const int MIN_SIGNED = -8;
const int MAX_SIGNED = 7;

int binaryToDecimal(const std::string& bin) {
    int unsignedValue = std::stol(bin, nullptr, 2);
    int signMask = 1 << (BITS - 1); 
    
    if ((unsignedValue & signMask) != 0) {
        return unsignedValue - (1 << BITS);
    }
    return unsignedValue;
}

std::string decimalToBinaryString(int value) {
    std::string s = "";
    int mask = (1 << BITS) - 1;
    int maskedVal = value & mask;
    
    for (int i = BITS - 1; i >= 0; i--) {
        s += ((maskedVal >> i) & 1) ? "1" : "0";
    }
    return s;
}

// ==========================================
// MÁQUINA DE ESTADOS E INTERFACE
// ==========================================
std::string binA = "", binB = "";
char operation = ' ';
int currentState = 0; 

void update_display() {
    lcd_clear();
    lcd_loc(0x80); 

    if (currentState == 0) {
        std::string msg = "A(4b): " + binA;
        lcd_string(msg.c_str());
    } 
    else if (currentState == 1) {
        std::string msg = "A:" + binA + " Op(A-D,*)";
        lcd_string(msg.c_str());
    } 
    else if (currentState == 2) {
        char opChar = (operation == 'A') ? '+' : (operation == 'B') ? '-' : (operation == 'C') ? '*' : '/';
        std::string msg = "A" + std::string(1, opChar) + " B(4b): " + binB;
        lcd_string(msg.c_str());
    }
}

void calculate_and_display() {
    int valA = binaryToDecimal(binA);
    int valB = (operation != '*') ? binaryToDecimal(binB) : 0;
    
    long long fullResult = 0;
    bool overflow = false;
    
    auto start = std::chrono::high_resolution_clock::now();

    if (operation == 'A') { fullResult = valA + valB; }
    else if (operation == 'B') { fullResult = valA - valB; }
    else if (operation == 'C') { fullResult = valA * valB; }
    else if (operation == 'D') {
        if (valB == 0) overflow = true; 
        else fullResult = valA / valB;
    }
    else if (operation == '*') { 
        if (valA < 0) overflow = true;
        else {
            long long acc = 1;
            for (int i = 1; i <= valA; i++) {
                acc *= i;
                if (acc < MIN_SIGNED || acc > MAX_SIGNED) { overflow = true; break; }
            }
            fullResult = acc;
        }
    }

    if (fullResult < MIN_SIGNED || fullResult > MAX_SIGNED) overflow = true;
    
    auto end = std::chrono::high_resolution_clock::now();
    long long timeSpent = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();

    lcd_clear();
    lcd_loc(0x80); 
    if (overflow && operation == 'D' && valB == 0) {
        lcd_string("Erro: Div/0");
    } else if (overflow) {
        lcd_string("Erro: OVERFLOW");
    } else {
        std::string binRes = "Res: " + decimalToBinaryString(fullResult);
        lcd_string(binRes.c_str());
        
        lcd_loc(0xC0); 
        std::string decTimeRes = "D:" + std::to_string(fullResult) + " T:" + std::to_string(timeSpent) + "us";
        lcd_string(decTimeRes.c_str());
    }
    currentState = 3; 
}

// ==========================================
// LOOP PRINCIPAL (Baremetal/Standalone)
// ==========================================
int main() {
    if (wiringPiSetupGpio() == -1) {
        std::cerr << "Erro ao inicializar WiringPi! Execute com 'sudo'." << std::endl;
        return 1;
    }

    lcd_init();
    setup_keypad();
    
    update_display(); 

    while (true) {
        char key = get_key();
        
        if (key != '\0') {
            if (key == '#') { 
                binA = ""; binB = ""; operation = ' ';
                currentState = 0;
                update_display();
                continue;
            }

            if (currentState == 0) {
                if ((key == '0' || key == '1') && binA.length() < BITS) {
                    binA += key;
                    update_display();
                    
                    if (binA.length() == BITS) {
                        currentState = 1;
                        update_display();
                    }
                }
            } 
            else if (currentState == 1) {
                if (key == 'A' || key == 'B' || key == 'C' || key == 'D' || key == '*') {
                    operation = key;
                    if (operation == '*') { 
                        calculate_and_display();
                    } else {
                        currentState = 2;
                        update_display();
                    }
                }
            } 
            else if (currentState == 2) {
                if ((key == '0' || key == '1') && binB.length() < BITS) {
                    binB += key;
                    update_display();
                    
                    if (binB.length() == BITS) {
                        calculate_and_display();
                    }
                }
            }
        }
        delay(30); 
    }
    return 0;
}