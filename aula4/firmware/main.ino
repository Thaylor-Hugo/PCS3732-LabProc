#include <WiFi.h>
#include <WebServer.h>

const char* ssid = "ESP32-Calc-GH";
const char* password = "12345678";

#define LED_BIT0 4
#define LED_BIT1 5
#define LED_BIT2 6
#define LED_BIT3 7

WebServer server(80);

void setup() {
  Serial.begin(115200);
  
  pinMode(LED_BIT0, OUTPUT);
  pinMode(LED_BIT1, OUTPUT);
  pinMode(LED_BIT2, OUTPUT);
  pinMode(LED_BIT3, OUTPUT);
  
  WiFi.softAP(ssid, password);
  Serial.print("AP IP address: ");
  Serial.println(WiFi.softAPIP());
  
  server.on("/calc", handleCalc);
  server.on("/", handleRoot);
  
  server.begin();
  Serial.println("HTTP server started");
}

void loop() {
  server.handleClient();
}

void handleRoot() {
  server.send(200, "text/plain", "Calculadora 4-bits running");
}

void handleCalc() {
  int valA = strtol(server.arg("a").c_str(), NULL, 2);
  int valB = 0;
  if (server.hasArg("b")) valB = strtol(server.arg("b").c_str(), NULL, 2);
  String op = server.arg("op");
  int bits = 4;
  if (server.hasArg("bits")) {
    bits = server.arg("bits").toInt();
    if (bits < 2) bits = 2;
    if (bits > 16) bits = 16;
  }
  int mask = (bits >= 32) ? -1 : ((1 << bits) - 1);
  int signMask = 1 << (bits - 1);
  int minSigned = -(1 << (bits - 1));
  int maxSigned = (1 << (bits - 1)) - 1;
  
  int resultado;
  
  // apply mask to operands
  valA = valA & mask;
  valB = valB & mask;

  bool overflow = false;
  long full = 0;
  if (op == "add") {
    int sA = (valA & signMask) ? (valA - (1 << bits)) : valA;
    int sB = (valB & signMask) ? (valB - (1 << bits)) : valB;
    full = (long)sA + (long)sB;
    if (full < minSigned || full > maxSigned) overflow = true;
    resultado = ((int)full) & mask;
  } else if (op == "sub") {
    int sA = (valA & signMask) ? (valA - (1 << bits)) : valA;
    int sB = (valB & signMask) ? (valB - (1 << bits)) : valB;
    full = (long)sA - (long)sB;
    if (full < minSigned || full > maxSigned) overflow = true;
    resultado = ((int)full) & mask;
  } else if (op == "mul") {
    int sA = (valA & signMask) ? (valA - (1 << bits)) : valA;
    int sB = (valB & signMask) ? (valB - (1 << bits)) : valB;
    full = (long)sA * (long)sB;
    if (full < minSigned || full > maxSigned) overflow = true;
    resultado = ((int)full) & mask;
  } else if (op == "fat") {
    int sA = (valA & signMask) ? (valA - (1 << bits)) : valA;
    if (sA < 0) {
      // factorial undefined for negative in this context -> overflow
      overflow = true;
      resultado = 0;
    } else {
      long acc = 1;
      for (int i = 1; i <= sA; i++) {
        acc *= i;
        if (acc < minSigned || acc > maxSigned) overflow = true;
      }
      resultado = ((int)acc) & mask;
    }
  }

  resultado = resultado & 0x0F;
  
  digitalWrite(LED_BIT0, (resultado >> 0) & 0x01);
  digitalWrite(LED_BIT1, (resultado >> 1) & 0x01);
  digitalWrite(LED_BIT2, (resultado >> 2) & 0x01);
  digitalWrite(LED_BIT3, (resultado >> 3) & 0x01);

  server.sendHeader("Access-Control-Allow-Origin", "*");
  
  String response = "{\"resultado\":" + String(resultado) + 
                    ",\"overflow\":" + (overflow ? "true" : "false") + "}";
  server.send(200, "application/json", response);
}