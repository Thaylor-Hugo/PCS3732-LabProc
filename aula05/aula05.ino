#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <ESP32Servo.h>
#include <WiFi.h>

namespace {
constexpr char kApSsid[] = "ESP32-Grupo-H";
constexpr char kApPassword[] = "12345678";

constexpr uint8_t kLedPin = 4;
constexpr uint8_t kServoPin = 5;

constexpr uint32_t kLedFrequency = 5000;
constexpr uint8_t kLedResolutionBits = 16;
constexpr uint32_t kLedMinFrequency = 500;
constexpr uint32_t kLedMaxFrequency = 10000;

constexpr uint16_t kServoMinUs = 500;
constexpr uint16_t kServoMaxUs = 2500;

AsyncWebServer server(80);
Servo servoMotor;

int currentLedDuty = 0;
int currentServoAngle = 0;
uint32_t currentLedFrequency = kLedFrequency;

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

int readServoAngle(AsyncWebServerRequest *request) {
	if (!request->hasParam("angle")) {
		return -1;
	}

	const int value = request->getParam("angle")->value().toInt();
	if (value < 0) {
		return 0;
	}

	if (value > 180) {
		return 180;
	}

	return value;
}

uint32_t readLedFrequency(AsyncWebServerRequest *request) {
	if (!request->hasParam("pwm_frequency")) {
		return currentLedFrequency;
	}

	const int value = request->getParam("pwm_frequency")->value().toInt();
	if (value < static_cast<int>(kLedMinFrequency)) {
		return kLedMinFrequency;
	}

	if (value > static_cast<int>(kLedMaxFrequency)) {
		return kLedMaxFrequency;
	}

	return static_cast<uint32_t>(value);
}

void applyLedDuty(int dutyCycle, uint32_t frequencyHz) {
	currentLedDuty = clampPercent(dutyCycle);
	currentLedFrequency = frequencyHz;
	ledcAttach(kLedPin, currentLedFrequency, kLedResolutionBits);
	const uint32_t maxDuty = (1UL << kLedResolutionBits) - 1;
	const uint32_t pwmValue = map(currentLedDuty, 0, 100, 0, maxDuty);
	ledcWrite(kLedPin, pwmValue);
}

void applyServoAngle(int angle) {
	currentServoAngle = angle;
	servoMotor.write(currentServoAngle);
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

	const uint32_t frequencyHz = readLedFrequency(request);
	applyLedDuty(dutyCycle, frequencyHz);
	AsyncWebServerResponse *response = request->beginResponse(200, "application/json", String("{\"led\":") + currentLedDuty + String(",\"pwm_frequency\":") + currentLedFrequency + "}");
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

	const int angle = readServoAngle(request);
	if (angle < 0) {
		AsyncWebServerResponse *response = request->beginResponse(400, "text/plain", "Missing angle");
		addCorsHeaders(response);
		request->send(response);
		return;
	}

	applyServoAngle(angle);
	AsyncWebServerResponse *response = request->beginResponse(200, "application/json", String("{\"servo_angle\":") + currentServoAngle + "}");
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
	ledcAttach(kLedPin, kLedFrequency, kLedResolutionBits);

	servoMotor.setPeriodHertz(50);
	servoMotor.attach(kServoPin, kServoMinUs, kServoMaxUs);

	applyLedDuty(0, kLedFrequency);
	applyServoAngle(0);

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
