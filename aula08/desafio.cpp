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
#define I2C_ADDR 0x27 // Endereço I2C padrão (Pode ser 0x3F dependendo do display)
#define LCD_CHR  1    // Modo de Envio de Dado (Caractere)
#define LCD_CMD  0    // Modo de Envio de Comando
#define LCD_BACKLIGHT 0x08
#define LCD_ENABLE    0b00000100

int lcd_fd; // File descriptor para o barramento I2C

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
    lcd_byte(0x06, LCD_CMD); // Modo de incremento de cursor
    lcd_byte(0x0C, LCD_CMD); // Display ON, Cursor OFF
    lcd_byte(0x28, LCD_CMD); // Modo 4-bit, 2 linhas
    lcd_byte(0x01, LCD_CMD); // Limpar display
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
// Numeração BCM padrão do Raspberry Pi
const int ROWS[4] = {16, 20, 21, 26}; 
const int COLS[4] = {19, 13, 6, 5}; 

char keys[4][4] = {
  {'1', '2', '3', 'A'}, // A = Add (+)
  {'4', '5', '6', 'B'}, // B = Sub (-)
  {'7', '8', '9', 'C'}, // C = Mult (*)
  {'*', '0', '#', 'D'}  // D = Div (/), * = Fat (!)
};

void setup_keypad() {
    // [CORREÇÃO DO RASPBERRY PI OS] 
    // Força o acionamento do resistor de Pull-Up interno para evitar "leituras fantasmas"
    system("raspi-gpio set 19,13,6,5 pu 2>/dev/null || pinctrl set 19,13,6,5 pu 2>/dev/null");

    for (int i = 0; i < 4; i++) {
        pinMode(ROWS[i], OUTPUT);
        digitalWrite(ROWS[i], HIGH); // Linhas começam desativadas (HIGH)
        
        pinMode(COLS[i], INPUT);
        pullUpDnControl(COLS[i], PUD_UP); // Mantido para retrocompatibilidade
    }
}

char get_key() {
    for (int r = 0; r < 4; r++) {
        digitalWrite(ROWS[r], LOW); // Ativa a linha atual (nível baixo)
        
        for (int c = 0; c < 4; c++) {
            if (digitalRead(COLS[c]) == LOW) {
                delay(30); // Filtro de ruído (Debounce)
                
                if (digitalRead(COLS[c]) == LOW) { // Confirma o aperto
                    char pressedKey = keys[r][c];
                    
                    // Trava o loop até o usuário SOLTAR o botão
                    // Isso impede o bug de imprimir a mesma tecla infinitamente
                    while(digitalRead(COLS[c]) == LOW) {
                        delay(10);
                    }
                    
                    digitalWrite(ROWS[r], HIGH); // Restaura a linha antes de sair
                    return pressedKey;
                }
            }
        }
        digitalWrite(ROWS[r], HIGH); // Desativa a linha para ler a próxima
    }
    return '\0'; // Nenhuma tecla pressionada
}

// ==========================================
// LÓGICA DE CÁLCULO E MATEMÁTICA (4 BITS)
// ==========================================
const int BITS = 4;
const int MIN_SIGNED = -8;
const int MAX_SIGNED = 7;

int binaryToDecimal(const std::string& bin) {
    int unsignedValue = std::stol(bin, nullptr, 2);
    int signMask = 1 << (BITS - 1); // Extrai o MSB (Bit de sinal)
    
    // Aplica regra de Complemento de Dois se o número for negativo
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
int currentState = 0; // 0=Lendo A, 1=Lendo OpCode, 2=Lendo B, 3=Exibindo Resultado

void update_display() {
    lcd_clear();
    lcd_loc(0x80); // Posiciona cursor na Linha 1

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
    
    // Benchmark local ARM
    auto start = std::chrono::high_resolution_clock::now();

    if (operation == 'A') { fullResult = valA + valB; }
    else if (operation == 'B') { fullResult = valA - valB; }
    else if (operation == 'C') { fullResult = valA * valB; }
    else if (operation == 'D') {
        if (valB == 0) overflow = true; // Exceção: Divisão por Zero
        else fullResult = valA / valB;
    }
    else if (operation == '*') { // Fatorial
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

    // Checagem global de limite (Overflow)
    if (fullResult < MIN_SIGNED || fullResult > MAX_SIGNED) overflow = true;
    
    auto end = std::chrono::high_resolution_clock::now();
    long long timeSpent = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();

    // Renderização do Resultado Final
    lcd_clear();
    lcd_loc(0x80); // Linha 1
    if (overflow && operation == 'D' && valB == 0) {
        lcd_string("Erro: Div/0");
    } else if (overflow) {
        lcd_string("Erro: OVERFLOW");
    } else {
        std::string binRes = "Res: " + decimalToBinaryString(fullResult);
        lcd_string(binRes.c_str());
        
        lcd_loc(0xC0); // Linha 2
        std::string decTimeRes = "D:" + std::to_string(fullResult) + " T:" + std::to_string(timeSpent) + "us";
        lcd_string(decTimeRes.c_str());
    }
    currentState = 3; // Aguarda o usuário apertar '#' para resetar
}

// ==========================================
// LOOP PRINCIPAL (Baremetal/Standalone)
// ==========================================
int main() {
    // Inicializa a WiringPi usando a numeração BCM
    if (wiringPiSetupGpio() == -1) {
        std::cerr << "Erro ao inicializar WiringPi! Execute com 'sudo'." << std::endl;
        return 1;
    }

    lcd_init();
    setup_keypad();
    
    update_display(); // Desenha a tela inicial

    while (true) {
        char key = get_key();
        
        if (key != '\0') {
            // A tecla '#' atua como RESET em qualquer estado do sistema
            if (key == '#') { 
                binA = ""; binB = ""; operation = ' ';
                currentState = 0;
                update_display();
                continue;
            }

            if (currentState == 0) {
                // Aceita apenas '0' ou '1' (Binário) limitando a 4 bits
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
                        // Fatorial ignora o operando B e calcula direto
                        calculate_and_display();
                    } else {
                        currentState = 2;
                        update_display();
                    }
                }
            } 
            else if (currentState == 2) {
                // Aceita apenas '0' ou '1' (Binário) limitando a 4 bits
                if ((key == '0' || key == '1') && binB.length() < BITS) {
                    binB += key;
                    update_display();
                    
                    if (binB.length() == BITS) {
                        calculate_and_display();
                    }
                }
            }
        }
        delay(30); // Alivia a carga de processamento na CPU do Raspberry
    }
    return 0;
}