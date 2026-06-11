#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <WiFi.h>

namespace {
constexpr char kApSsid[] = "ESP32-Grupo-H";
constexpr char kApPassword[] = "12345678";

constexpr uint8_t kLedPin = 2;
constexpr uint8_t kServoPin = 18;

constexpr uint8_t kLedChannel = 0;
constexpr uint8_t kServoChannel = 1;

constexpr uint32_t kLedFrequency = 5000;
constexpr uint8_t kLedResolutionBits = 8;

constexpr uint32_t kServoFrequency = 50;
constexpr uint8_t kServoResolutionBits = 16;

constexpr uint16_t kServoMinUs = 500;
constexpr uint16_t kServoMaxUs = 2500;

AsyncWebServer server(80);

int currentLedDuty = 0;
int currentServoDuty = 0;

void addCorsHeaders(AsyncWebServerResponse *response) {
	response->addHeader("Access-Control-Allow-Origin", "*");
	response->addHeader("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
	response->addHeader("Access-Control-Allow-Headers", "Content-Type");
	response->addHeader("Access-Control-Allow-Private-Network", "true");
}

int clampPercent(int value) {
	if (value < 0) {
		return 0;
	}

	if (value > 100) {
		return 100;
	}

	return value;
}

int readDutyCycle(AsyncWebServerRequest *request) {
	if (!request->hasParam("duty_cycle")) {
		return -1;
	}

	return clampPercent(request->getParam("duty_cycle")->value().toInt());
}

void applyLedDuty(int dutyCycle) {
	currentLedDuty = clampPercent(dutyCycle);
	const int pwmValue = map(currentLedDuty, 0, 100, 0, 255);
	ledcWrite(kLedChannel, pwmValue);
}

void applyServoDuty(int dutyCycle) {
	currentServoDuty = clampPercent(dutyCycle);
	const int pulseWidthUs = map(currentServoDuty, 0, 100, kServoMinUs, kServoMaxUs);
	const uint32_t maxDuty = (1UL << kServoResolutionBits) - 1;
	const uint32_t duty = (static_cast<uint32_t>(pulseWidthUs) * maxDuty) / 20000UL;
	ledcWrite(kServoChannel, duty);
}

void handleRoot(AsyncWebServerRequest *request) {
	AsyncWebServerResponse *response = request->beginResponse(200, "text/plain", "ESP32 controller is running");
	addCorsHeaders(response);
	request->send(response);
}

void handleLed(AsyncWebServerRequest *request) {
	if (request->method() == HTTP_OPTIONS) {
		AsyncWebServerResponse *response = request->beginResponse(204);
		addCorsHeaders(response);
		request->send(response);
		return;
	}

	if (request->method() != HTTP_POST) {
		AsyncWebServerResponse *response = request->beginResponse(405, "text/plain", "Use POST");
		addCorsHeaders(response);
		request->send(response);
		return;
	}

	const int dutyCycle = readDutyCycle(request);
	if (dutyCycle < 0) {
		AsyncWebServerResponse *response = request->beginResponse(400, "text/plain", "Missing duty_cycle");
		addCorsHeaders(response);
		request->send(response);
		return;
	}

	applyLedDuty(dutyCycle);
	AsyncWebServerResponse *response = request->beginResponse(200, "application/json", String("{\"led\":") + currentLedDuty + "}");
	addCorsHeaders(response);
	request->send(response);
}

void handleServo(AsyncWebServerRequest *request) {
	if (request->method() == HTTP_OPTIONS) {
		AsyncWebServerResponse *response = request->beginResponse(204);
		addCorsHeaders(response);
		request->send(response);
		return;
	}

	if (request->method() != HTTP_POST) {
		AsyncWebServerResponse *response = request->beginResponse(405, "text/plain", "Use POST");
		addCorsHeaders(response);
		request->send(response);
		return;
	}

	const int dutyCycle = readDutyCycle(request);
	if (dutyCycle < 0) {
		AsyncWebServerResponse *response = request->beginResponse(400, "text/plain", "Missing duty_cycle");
		addCorsHeaders(response);
		request->send(response);
		return;
	}

	applyServoDuty(dutyCycle);
	AsyncWebServerResponse *response = request->beginResponse(200, "application/json", String("{\"servo\":") + currentServoDuty + "}");
	addCorsHeaders(response);
	request->send(response);
}

void handleNotFound(AsyncWebServerRequest *request) {
	if (request->method() == HTTP_OPTIONS) {
		AsyncWebServerResponse *response = request->beginResponse(204);
		addCorsHeaders(response);
		request->send(response);
		return;
	}

	AsyncWebServerResponse *response = request->beginResponse(404, "text/plain", "Not found");
	addCorsHeaders(response);
	request->send(response);
}
}  // namespace

void setup() {
	Serial.begin(115200);
	delay(200);

	pinMode(kLedPin, OUTPUT);
	ledcSetup(kLedChannel, kLedFrequency, kLedResolutionBits);
	ledcAttachPin(kLedPin, kLedChannel);

	ledcSetup(kServoChannel, kServoFrequency, kServoResolutionBits);
	ledcAttachPin(kServoPin, kServoChannel);

	applyLedDuty(0);
	applyServoDuty(0);

	WiFi.mode(WIFI_AP);
	WiFi.softAP(kApSsid, kApPassword);

	Serial.println();
	Serial.println("ESP32 access point started");
	Serial.print("SSID: ");
	Serial.println(kApSsid);
	Serial.print("IP address: ");
	Serial.println(WiFi.softAPIP());

	server.on("/", HTTP_GET, handleRoot);
	server.on("/led", HTTP_POST, handleLed);
	server.on("/led", HTTP_OPTIONS, handleLed);
	server.on("/servo", HTTP_POST, handleServo);
	server.on("/servo", HTTP_OPTIONS, handleServo);
	server.onNotFound(handleNotFound);
	server.begin();
}

void loop() {
}
