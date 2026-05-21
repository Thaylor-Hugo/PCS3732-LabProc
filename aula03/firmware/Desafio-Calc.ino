#define LED_BIT0 13
#define LED_BIT1 12
#define LED_BIT2 14
#define LED_BIT3 27  

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  pinMode(LED_BIT0, OUTPUT);
  pinMode(LED_BIT1, OUTPUT);
  pinMode(LED_BIT2, OUTPUT);
  pinMode(LED_BIT3, OUTPUT);
  
  Serial.println("\n=== CALCULADORA 4-BITS (COMPLEMENTO DE UM) ===");
  Serial.println("Formato: AAAA op BBBB");
  Serial.println("Operadores: + ou -");
  Serial.println("Exemplo: 0110 + 0010");
  Serial.println("Exemplo: 1000 - 0001");
  Serial.println("==========================================\n");
}

void loop() {
  if (Serial.available() > 0) {
    String input = Serial.readStringUntil('\n');
    input.trim();
    
    int opIndex = input.indexOf('+');
    if (opIndex == -1) {
      opIndex = input.indexOf('-');
    }
    
    if (opIndex == -1 || opIndex < 4) {
      Serial.println("Erro: Formato inválido! Use: AAAA op BBBB\n");
      return;
    }
    
    String strA = input.substring(0, 4);
    char op = input.charAt(opIndex);
    String strB = input.substring(opIndex + 1);
    
    strA.trim();
    strB.trim();
    
    if (strA.length() != 4 || strB.length() != 4) {
      Serial.println("Erro: Operandos devem ter exatamente 4 bits!\n");
      return;
    }
    
    int valA = strtol(strA.c_str(), NULL, 2);
    int valB = strtol(strB.c_str(), NULL, 2);
    int resultado = 0;
    bool overflow = false;
    
    if (op == '+') {
      resultado = valA + valB;
      
      if (((valA & 0x08) == 0 && (valB & 0x08) == 0 && (resultado & 0x10) == 0 && (resultado & 0x08) != 0) ||
          ((valA & 0x08) != 0 && (valB & 0x08) != 0 && (resultado & 0x10) != 0 && (resultado & 0x08) == 0)) {
        overflow = true;
      }
    } else if (op == '-') {
      int invB = (~valB) & 0x0F;
      resultado = valA + invB;
      
      if (((valA & 0x08) == 0 && (invB & 0x08) == 0 && (resultado & 0x10) == 0 && (resultado & 0x08) != 0) ||
          ((valA & 0x08) != 0 && (invB & 0x08) != 0 && (resultado & 0x10) != 0 && (resultado & 0x08) == 0)) {
        overflow = true;
      }
    } else {
      Serial.println("Erro: Operador deve ser '+' ou '-'\n");
      return;
    }
    
    if (resultado > 0x0F) {
      resultado = (resultado & 0x0F) + 1;
    }
    
    resultado = resultado & 0x0F;
    
    digitalWrite(LED_BIT0, (resultado >> 0) & 0x01);
    digitalWrite(LED_BIT1, (resultado >> 1) & 0x01);
    digitalWrite(LED_BIT2, (resultado >> 2) & 0x01);
    digitalWrite(LED_BIT3, (resultado >> 3) & 0x01);
    
    Serial.println("\n--- RESULTADO (COMPLEMENTO DE UM) ---");
    Serial.print("A (binário): ");
    Serial.println(strA);
    Serial.print("B (binário): ");
    Serial.println(strB);
    Serial.print("Operação: ");
    Serial.println(op);
    Serial.print("Resultado: ");
    Serial.print((resultado >> 3) & 0x01);
    Serial.print((resultado >> 2) & 0x01);
    Serial.print((resultado >> 1) & 0x01);
    Serial.println((resultado >> 0) & 0x01);
    Serial.print("Overflow: ");
    Serial.println(overflow ? "SIM " : "NÃO ");
    Serial.println();
  }
}