#include <iostream>
#include <string>
#include <wiringPi.h>
#include <wiringPiI2C.h>
#include <unistd.h>

// ==========================================
// CONFIGURAÇÕES DO DISPLAY LCD I2C
// ==========================================
#define I2C_ADDR 0x27 
#define LCD_CHR  1 
#define LCD_CMD  0 
#define LINE1  0x80 
#define LINE2  0xC0 
#define LCD_BACKLIGHT   0x08  
#define ENABLE  0b00000100 

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

// ==========================================
// CONFIGURAÇÕES DO TECLADO MATRICIAL 4X4
// ==========================================
const int ROW[4] = {16, 20, 21, 26}; // BCM
const int COL[4] = {19, 13, 6, 5};   // BCM

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
        pullUpDnControl(COL[i], PUD_UP); // Mantém como fallback
    }
}

char keypad_scan() {
    for (int r=0; r<4; r++) {
        digitalWrite(ROW[r], LOW);
        for (int c=0; c<4; c++) {
            if (digitalRead(COL[c]) == LOW) {
                delay(20); 
                if (digitalRead(COL[c]) == LOW) {
                    while (digitalRead(COL[c]) == LOW) { delay(10); } 
                    digitalWrite(ROW[r], HIGH);
                    return keys[r][c];
                }
            }
        }
        digitalWrite(ROW[r], HIGH);
    }
    return '\0';
}

int main() {
    if (wiringPiSetupGpio() == -1) {
        std::cerr << "Erro ao inicializar wiringPi\n";
        return 1;
    }
    
    lcd_init();
    keypad_init();

    lcd_clear();
    lcd_loc(LINE1);
    lcd_print("Teste de Teclado");
    
    std::cout << "===========================" << std::endl;
    std::cout << "  MODO DE TESTE DO KEYPAD  " << std::endl;
    std::cout << "===========================" << std::endl;
    std::cout << "Pressione as teclas físicas para verificar o mapeamento." << std::endl;

    while(true) {
        char k = keypad_scan();
        if (k != '\0') {
            std::cout << "Lido pela matriz GPIO: " << k << std::endl;
            
            lcd_loc(LINE2);
            char str[17];
            snprintf(str, sizeof(str), "Tecla: %c        ", k);
            lcd_print(str);
        }
        delay(50);
    }
    return 0;
}
