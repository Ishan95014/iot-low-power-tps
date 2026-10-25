#include <Arduino.h>

// Adapted from pcourbin-teaching/esp32_examples, ESP32_Arduino_ADC.
constexpr uint8_t ADC_PIN = 36;  // ADC1_CH0, labelled VP on the Joy-IT board.
// Binome identification for serial evidence.
constexpr char PREFIX[] = "[Binome 4 - Ishan & Lison]";

void setup() {
    Serial.begin(115200);
    analogReadResolution(12);  // Raw readings: 0 to 4095.
    analogSetPinAttenuation(ADC_PIN, ADC_11db);
}

void loop() {
    const int raw = analogRead(ADC_PIN);
    Serial.printf("%s LDR raw=%d\n", PREFIX, raw);
    delay(1000);
}
