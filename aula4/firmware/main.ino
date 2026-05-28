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
  int valB = strtol(server.arg("b").c_str(), NULL, 2);
  String op = server.arg("op");
  
  int resultado = (op == "add") ? (valA + valB) : (valA - valB);
  
  bool overflow = false;
  if (op == "add") {
    if (((valA & 0x08) == 0 && (valB & 0x08) == 0 && (resultado & 0x08) != 0) ||
        ((valA & 0x08) != 0 && (valB & 0x08) != 0 && (resultado & 0x08) == 0)) {
      overflow = true;
    }
  } else {
    if (((valA & 0x08) == 0 && (valB & 0x08) != 0 && (resultado & 0x08) != 0) ||
        ((valA & 0x08) != 0 && (valB & 0x08) == 0 && (resultado & 0x08) == 0)) {
      overflow = true;
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