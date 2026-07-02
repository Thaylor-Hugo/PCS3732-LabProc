#include <WebServer.h>
#include <WiFi.h>

namespace {

constexpr char kApSsid[] = "ESP32-Grupo-H";
constexpr char kApPassword[] = "12345678";

constexpr uint8_t kLdrPin = 2;
constexpr uint8_t kButtonPin = 3; // Pin assigned to the button

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

enum TrafficState {
    TRAFFIC_GREEN,
    TRAFFIC_YELLOW,
    TRAFFIC_RED
};

unsigned long lastBlinkTime = 0;
bool ledState = false;

// Variables used inside ISR for button debounce and interrupt control
volatile bool isRedActive = false;
volatile unsigned long redStartTime = 0;
volatile unsigned long lastDebounceTime = 0;
constexpr unsigned long kDebounceDelayMs = 100;

volatile TrafficState currentTrafficState = TRAFFIC_GREEN;
unsigned long trafficStateStartTime = 0;

/**
 * Interrupt Service Routine (ISR) for the button press.
 * Runs on rising edge (button pressed to 3.3v).
 */
void IRAM_ATTR handleButtonInterrupt() {
    unsigned long currentTime = millis();
    // Simple software debouncer
    if (currentTime - lastDebounceTime > kDebounceDelayMs) {
        lastDebounceTime = currentTime;
        // Ignore if override is already active or if the traffic light is currently RED
        if (!isRedActive && currentTrafficState != TRAFFIC_RED) {
            isRedActive = true;
            redStartTime = currentTime;
        }
    }
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

    // Initialize button pin with internal pullup
    pinMode(kButtonPin, INPUT_PULLDOWN);
    // Attach rising edge interrupt to trigger handleButtonInterrupt when button goes high
    attachInterrupt(digitalPinToInterrupt(kButtonPin), handleButtonInterrupt, RISING);
    Serial.print("Button Pin: GPIO ");
    Serial.println(kButtonPin);

    // Initialize built-in LED (NeoPixel) pin
    pinMode(LED_BUILTIN, OUTPUT);
    neopixelWrite(LED_BUILTIN, 0, 0, 0);

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

    if (isRedActive) {
        // Emergency Red Light
        neopixelWrite(LED_BUILTIN, 100, 0, 0);
        ledState = false;

        if (millis() - redStartTime >= 3000) {
            isRedActive = false;
            const int exitLuminosity = analogRead(kLdrPin);
            if (exitLuminosity >= 2800) {
                lastBlinkTime = millis();
                ledState = false;
                neopixelWrite(LED_BUILTIN, 0, 0, 0);
            } else {
                currentTrafficState = TRAFFIC_GREEN;
                trafficStateStartTime = millis();
                neopixelWrite(LED_BUILTIN, 0, 100, 0); // Green
            }
        }
        return;
    }

    static bool wasLowLight = true;
    const int currentLuminosity = analogRead(kLdrPin);

    if (currentLuminosity >= 2800) {
        if (!wasLowLight) {
            wasLowLight = true;
            currentTrafficState = TRAFFIC_GREEN;
        }

        // Night Mode
        const unsigned long currentMillis = millis();
        if (currentMillis - lastBlinkTime >= 1000) {
            lastBlinkTime = currentMillis;
            ledState = !ledState;
            if (ledState) {
                neopixelWrite(LED_BUILTIN, 100, 100, 0); // Yellow
            } else {
                neopixelWrite(LED_BUILTIN, 0, 0, 0);
            }
        }
    } else {
        if (wasLowLight) {
            wasLowLight = false;
            currentTrafficState = TRAFFIC_GREEN;
            trafficStateStartTime = millis();
            neopixelWrite(LED_BUILTIN, 0, 100, 0); // Green
        }

        const unsigned long elapsed = millis() - trafficStateStartTime;

        // Normal traffic light flow
        if (currentTrafficState == TRAFFIC_GREEN) {
            neopixelWrite(LED_BUILTIN, 0, 100, 0); // Green
            if (elapsed >= 4000) {
                currentTrafficState = TRAFFIC_YELLOW;
                trafficStateStartTime = millis();
                neopixelWrite(LED_BUILTIN, 100, 100, 0); // Yellow
            }
        } else if (currentTrafficState == TRAFFIC_YELLOW) {
            neopixelWrite(LED_BUILTIN, 100, 100, 0); // Yellow
            if (elapsed >= 1000) {
                currentTrafficState = TRAFFIC_RED;
                trafficStateStartTime = millis();
                neopixelWrite(LED_BUILTIN, 100, 0, 0); // Red
            }
        } else if (currentTrafficState == TRAFFIC_RED) {
            neopixelWrite(LED_BUILTIN, 100, 0, 0); // Red
            if (elapsed >= 3000) {
                currentTrafficState = TRAFFIC_GREEN;
                trafficStateStartTime = millis();
                neopixelWrite(LED_BUILTIN, 0, 100, 0); // Green
            }
        }
    }
}
