#include <Arduino.h>
#include "esp_sleep.h"
#include "esp_system.h"

constexpr uint8_t LED_PIN = 2;
constexpr uint32_t ON_MS = 200;
constexpr uint32_t OFF_MS = 1800;
constexpr uint64_t DEEP_SLEEP_US = uint64_t(OFF_MS) * 1000ULL;
constexpr char PREFIX[] = "[Binome 04 - Lison GEORGE & Ishan BAICHOO]";

// Ordinary global first: observe its reinitialization after deep sleep.
uint32_t bootCount = 0;

void setup() {
    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, LOW);
    Serial.begin(115200);
    ++bootCount;
    Serial.printf("%s boot #%lu - reset=%d mode=%d\n", PREFIX,
                  static_cast<unsigned long>(bootCount),
                  static_cast<int>(esp_reset_reason()), TP3_MODE);
    Serial.flush();
}

void loop() {
    digitalWrite(LED_PIN, HIGH);
    delay(ON_MS);
    digitalWrite(LED_PIN, LOW);

#if TP3_MODE == 0
    delay(OFF_MS);
#elif TP3_MODE == 1
    esp_sleep_enable_timer_wakeup(uint64_t(OFF_MS) * 1000ULL);
    ESP_ERROR_CHECK(esp_light_sleep_start());
#elif TP3_MODE == 2
    // Same 1800 ms off interval in every mode; deep sleep also incurs boot overhead.
    esp_sleep_enable_timer_wakeup(DEEP_SLEEP_US);
    esp_deep_sleep_start();
#else
#error Unsupported TP3_MODE
#endif
}
