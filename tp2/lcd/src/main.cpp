#include "../../group.h"
#include <Arduino.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

LiquidCrystal_I2C lcd27(0x27, 16, 2);
LiquidCrystal_I2C lcd3f(0x3F, 16, 2);
LiquidCrystal_I2C *lcd = nullptr;
uint32_t lastSecond = UINT32_MAX;

void setup() {
    Serial.begin(115200);
    delay(300);
    Serial.printf("\n===== %s | TP2 =====\n", GROUP_NAME);
    Wire.begin(21, 22);
    delay(500);
    Serial.println("TP2.1.2 - LCD greeting and uptime");
    for (uint8_t address = 1; address < 127; ++address) {
        Wire.beginTransmission(address);
        if (Wire.endTransmission() == 0) {
            Serial.printf("I2C device: 0x%02X\n", address);
            if (address == 0x27) lcd = &lcd27;
            if (address == 0x3F && lcd == nullptr) lcd = &lcd3f;
        }
    }
    if (lcd == nullptr) {
        Serial.println("LCD not found at 0x27 or 0x3F. Check wiring/address.");
        return;
    }
    lcd->init();
    lcd->backlight();
    lcd->clear();
    lcd->setCursor(0, 0);
    lcd->print("Great IoT Tuto!");
    Serial.println("LCD ready. Static greeting: Great IoT Tuto!");
}

void loop() {
    if (lcd == nullptr) {
        delay(1000);
        return;
    }
    const uint32_t second = millis() / 1000;
    if (second == lastSecond) return;
    lastSecond = second;

    char uptime[24];
    snprintf(uptime, sizeof(uptime), "Launched for %lus", static_cast<unsigned long>(second));
    lcd->setCursor(0, 1);
    for (size_t column = 0; column < 16; ++column) lcd->print(' ');
    lcd->setCursor(0, 1);
    lcd->print(uptime);
    Serial.printf("[%s] %s | Great IoT Tuto!\n", GROUP_NAME, uptime);
}
