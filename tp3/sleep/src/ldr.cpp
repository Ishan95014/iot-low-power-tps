#include <Arduino.h>
#include "driver/gpio.h"
#include "esp_sleep.h"

// Valeurs du TP1 
constexpr uint8_t LDR_PIN = 36;
constexpr uint8_t LED_PIN = 2;
// Calibration 
constexpr int LDR_THRESHOLD = 2000;

#if TP3_LDR_SLEEP
RTC_DATA_ATTR bool previousLedOn = false;
#endif

void setup() {
    pinMode(LED_PIN, OUTPUT);
#if TP3_LDR_SLEEP
    digitalWrite(LED_PIN, previousLedOn ? HIGH : LOW);
    ESP_ERROR_CHECK(gpio_hold_dis(static_cast<gpio_num_t>(LED_PIN)));
    gpio_deep_sleep_hold_dis();
#else
    digitalWrite(LED_PIN, LOW);
    Serial.begin(115200);
    Serial.println("[Binome 04 - Lison GEORGE & Ishan BAICHOO] LDR calibration, GPIO36 / VP");
#endif
    analogReadResolution(12);
    analogSetPinAttenuation(LDR_PIN, ADC_11db);
}

void loop() {
    const int raw = analogRead(LDR_PIN);
    const bool ledOn = raw > LDR_THRESHOLD;
    digitalWrite(LED_PIN, ledOn ? HIGH : LOW);

#if TP3_LDR_SLEEP

    previousLedOn = ledOn;
    ESP_ERROR_CHECK(gpio_hold_en(static_cast<gpio_num_t>(LED_PIN)));
    gpio_deep_sleep_hold_en();
    ESP_ERROR_CHECK(esp_sleep_enable_timer_wakeup(TP3_LDR_SLEEP_US));
    esp_deep_sleep_start();
#else
    static uint32_t lastPrint = 0;
    const uint32_t now = millis();
    if (now - lastPrint >= 250) {
        lastPrint = now;
        Serial.printf("LDR raw=%d threshold=%d LED=%d\n", raw, LDR_THRESHOLD, ledOn);
    }
    delay(1);
#endif
}
