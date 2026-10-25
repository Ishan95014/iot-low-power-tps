// See https://docs.espressif.com/projects/arduino-esp32/en/latest/api/adc.html
// Example of using ADC on ESP32 with Arduino framework
#include <Arduino.h>
#include <driver/adc.h>

void setup() {
    pinMode(2, OUTPUT);
    digitalWrite(2, LOW);
}

void loop() {
    digitalWrite(2, HIGH);
    delay(200);
    digitalWrite(2, LOW);
    delay(2000);
}

