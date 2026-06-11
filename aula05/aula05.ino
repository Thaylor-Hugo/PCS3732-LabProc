#include <ESP32Servo.h>
#include <WiFi.h>
#include <WebServer.h>

namespace {
constexpr char kApSsid[] = "ESP32-Grupo-H";
constexpr char kApPassword[] = "12345678";

constexpr uint8_t kLedPin = 4;
constexpr uint8_t kServoPin = 5;

constexpr uint32_t kLedFrequency = 5000;
constexpr uint8_t kLedResolutionBits = 12;
constexpr uint32_t kLedMinFrequency = 0;
constexpr uint32_t kLedMaxFrequency = 10000;

constexpr uint16_t kServoMinUs = 500;
constexpr uint16_t kServoMaxUs = 2500;

WebServer server(80);
Servo servoMotor;
ESP32PWM ledPwm;

int currentLedDuty = 0;
int currentServoAngle = 0;
uint32_t currentLedFrequency = kLedFrequency;

void addCorsHeaders() {
	server.sendHeader("Access-Control-Allow-Origin", "*");
	server.sendHeader("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
	server.sendHeader("Access-Control-Allow-Headers", "Content-Type");
	server.sendHeader("Access-Control-Allow-Private-Network", "true");
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

int readDutyCycle() {
	if (!server.hasArg("duty_cycle")) {
		return -1;
	}

	return clampPercent(server.arg("duty_cycle").toInt());
}

int readServoAngle() {
	if (!server.hasArg("angle")) {
		return -1;
	}

	const int value = server.arg("angle").toInt();
	if (value < 0) {
		return 0;
	}

	if (value > 180) {
		return 180;
	}

	return value;
}

uint32_t readLedFrequency() {
	if (!server.hasArg("pwm_frequency")) {
		return currentLedFrequency;
	}

	const int value = server.arg("pwm_frequency").toInt();
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
    const float dutyScaled = currentLedDuty / 100.0f;
    ledPwm.adjustFrequency(currentLedFrequency, dutyScaled);
}

void applyServoAngle(int angle) {
	currentServoAngle = angle;
	servoMotor.write(currentServoAngle);
}

void handleRoot() {
	addCorsHeaders();
	server.send(200, "text/plain", "ESP32 controller is running");
}

void handleLedOptions() {
	addCorsHeaders();
	server.send(204);
}

void handleServoOptions() {
	addCorsHeaders();
	server.send(204);
}

void handleLed() {
	if (server.method() != HTTP_POST) {
		addCorsHeaders();
		server.send(405, "text/plain", "Use POST");
		return;
	}

	const int dutyCycle = readDutyCycle();
	if (dutyCycle < 0) {
		addCorsHeaders();
		server.send(400, "text/plain", "Missing duty_cycle");
		return;
	}

	const uint32_t frequencyHz = readLedFrequency();
	applyLedDuty(dutyCycle, frequencyHz);
	addCorsHeaders();
	server.send(200, "application/json", String("{\"led\":") + currentLedDuty + String(",\"pwm_frequency\":") + currentLedFrequency + "}");
}

void handleServo() {
	if (server.method() != HTTP_POST) {
		addCorsHeaders();
		server.send(405, "text/plain", "Use POST");
		return;
	}

	const int angle = readServoAngle();
	if (angle < 0) {
		addCorsHeaders();
		server.send(400, "text/plain", "Missing angle");
		return;
	}

	applyServoAngle(angle);
	addCorsHeaders();
	server.send(200, "application/json", String("{\"servo_angle\":") + currentServoAngle + "}");
}

void handleNotFound() {
	if (server.method() == HTTP_OPTIONS) {
		addCorsHeaders();
		server.send(204);
		return;
	}

	addCorsHeaders();
	server.send(404, "text/plain", "Not found");
}
}  // namespace

void setup() {
	Serial.begin(115200);
	delay(200);

    ESP32PWM allocateTimer(0);
    ESP32PWM allocateTimer(1);

	servoMotor.setPeriodHertz(50);
	servoMotor.attach(kServoPin, kServoMinUs, kServoMaxUs);
	

    ledPwm.attachPin(kLedPin, kLedFrequency, kLedResolutionBits);

	applyServoAngle(0);
	applyLedDuty(0, kLedFrequency);

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
	server.on("/led", HTTP_OPTIONS, handleLedOptions);
	server.on("/servo", HTTP_POST, handleServo);
	server.on("/servo", HTTP_OPTIONS, handleServoOptions);
	server.onNotFound(handleNotFound);
	server.begin();
}

void loop() {
	server.handleClient();
}
