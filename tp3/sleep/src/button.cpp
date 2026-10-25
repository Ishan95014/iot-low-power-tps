#include <Arduino.h>
#include "driver/gpio.h"
#include "esp_sleep.h"

// Pull Down Interne defalut LOW
constexpr uint8_t BUTTON_PIN = 33;
constexpr uint8_t LED_PIN = 2;

void setup() {
    pinMode(BUTTON_PIN, INPUT_PULLDOWN);
    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, LOW);
    Serial.begin(115200);
    Serial.printf("[Binome 04 - Lison GEORGE & Ishan BAICHOO] button, light_sleep=%d\n",
                  TP3_BUTTON_SLEEP);
    Serial.flush();
#if TP3_BUTTON_SLEEP
    ESP_ERROR_CHECK(gpio_wakeup_enable(static_cast<gpio_num_t>(BUTTON_PIN), GPIO_INTR_HIGH_LEVEL));
    ESP_ERROR_CHECK(esp_sleep_enable_gpio_wakeup());
#endif
}

void loop() {
    const bool pressed = digitalRead(BUTTON_PIN) == HIGH;
    digitalWrite(LED_PIN, pressed ? HIGH : LOW);
#if TP3_BUTTON_SLEEP
    if (!pressed) {
        ESP_ERROR_CHECK(esp_light_sleep_start());
        return;
    }
#endif
    delay(1);
}
