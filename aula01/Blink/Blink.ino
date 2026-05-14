// the setup function runs once when you press reset or power the board
void setup() {
  // initialize digital pin LED_BUILTIN as an output.
  pinMode(LED_BUILTIN, OUTPUT);
}

// the loop function runs over and over again forever
void loop() {
  // AMARELO
  for(int i = 0; i < 5; i++) {
    neopixelWrite(LED_BUILTIN, 100, 100, 0);
    digitalWrite(LED_BUILTIN, HIGH);  // change state of the LED by setting the pin to the HIGH voltage level
    delay(100);
    digitalWrite(LED_BUILTIN, LOW);  // change state of the LED by setting the pin to the HIGH voltage level
    delay(100);
  }

  // VERMELHO
  neopixelWrite(LED_BUILTIN, 100, 0, 0);
  digitalWrite(LED_BUILTIN, HIGH);  // change state of the LED by setting the pin to the HIGH voltage level
  delay(4000);                      // wait for a second
  
  // VERDE
  neopixelWrite(LED_BUILTIN, 0, 100, 0);
  digitalWrite(LED_BUILTIN, HIGH);  // change state of the LED by setting the pin to the HIGH voltage level
  delay(3000);                      // wait for a second
  
}