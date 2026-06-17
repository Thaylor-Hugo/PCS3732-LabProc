#include <WebServer.h>
#include <WiFi.h>

namespace {

constexpr char kApSsid[] = "ESP32-Grupo-H";
constexpr char kApPassword[] = "12345678";

constexpr uint8_t kLdrPin = 34;

WebServer server(80);

void addCorsHeaders() {
	server.sendHeader("Access-Control-Allow-Origin", "*");
	server.sendHeader("Access-Control-Allow-Methods", "GET, OPTIONS");
	server.sendHeader("Access-Control-Allow-Headers", "Content-Type");
	server.sendHeader("Access-Control-Allow-Private-Network", "true");
}

void handleRoot() {
	addCorsHeaders();

	if (server.method() == HTTP_OPTIONS) {
		server.send(204);
		return;
	}

	const int ldrValue = analogRead(kLdrPin);
	const String jsonResponse = "{\"luminosity\":" + String(ldrValue) + "}";

	Serial.print("Request GET / - Reading LDR ADC value: ");
	Serial.println(ldrValue);

    server.send(200, "application/json", jsonResponse);
}

void handleRootOptions() {
	addCorsHeaders();
	server.send(204);
}

void handleNotFound() {
	addCorsHeaders();
    if (server.method() == HTTP_OPTIONS) {
        server.send(204);
        return;
    }
    server.send(404, "text/plain", "Endpoint not found");
}

} // namespace

void setup() {
    Serial.begin(115200);
    delay(200);
    Serial.println("\n--- ESP32 LDR LUMINOSITY SERVER ---");

    analogReadResolution(12);
    analogSetAttenuation(ADC_11db);

    pinMode(kLdrPin, INPUT);
    Serial.print("LDR Input Pin: GPIO ");
    Serial.println(kLdrPin);

    WiFi.mode(WIFI_AP);
    WiFi.softAP(kApSsid, kApPassword);

    Serial.println("WiFi Access Point configured:");
    Serial.print("  SSID:     ");
    Serial.println(kApSsid);
    Serial.print("  IP:       ");
    Serial.println(WiFi.softAPIP());

    server.on("/", HTTP_GET, handleRoot);
    server.on("/", HTTP_OPTIONS, handleRootOptions);
    server.onNotFound(handleNotFound);
    server.begin();
    Serial.println("HTTP Web Server started on port 80.");
}

void loop() {
    server.handleClient();
}
