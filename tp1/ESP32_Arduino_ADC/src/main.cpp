// See https://docs.espressif.com/projects/arduino-esp32/en/latest/api/adc.html
// Example of using ADC on ESP32 with Arduino framework
#include <Arduino.h>
#include <driver/adc.h>

void setup() {
    Serial.begin(115200);
    // Configure ADC on GPIO 36 (ADC1_CH0)
    analogSetPinAttenuation(36, (adc_attenuation_t)ADC_ATTEN_DB_12);  // Set attenuation to 12dB (max ~3.1V)
    analogSetWidth(12);                                               // Set ADC resolution to 12 bits (0-4095)
}

void loop() {
    // Read raw ADC value (0-4095)
    int value = analogRead(36);
    Serial.println("Value: " + String(value));

    // Convert raw value to millivolts (150-3100mV with 11dB or 12dB attenuation with ESP32)
    float voltage = (float)value * (3100.0 - 150.0) / 4095.0 + 150.0;

    // Directly read voltage in millivolts
    float direct_voltage = analogReadMilliVolts(36);

    // Print calculated and direct millivolt values
    Serial.println("Millivolts: " + String(voltage));
    Serial.println("Direct Millivolts: " + String(direct_voltage));

    delay(1000);  // Wait 1 second
}