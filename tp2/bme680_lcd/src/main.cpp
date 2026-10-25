#include "../../group.h"
#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_BME680.h>
#include <LiquidCrystal_I2C.h>

Adafruit_BME680 bme;
LiquidCrystal_I2C lcd27(0x27, 16, 2);
LiquidCrystal_I2C lcd3f(0x3F, 16, 2);
LiquidCrystal_I2C *lcd = nullptr;
bool sensorReady = false;

void displayLine(uint8_t row, const char *text) {
    if (lcd == nullptr) return;
    lcd->setCursor(0, row);
    bool ended = false;
    for (uint8_t column = 0; column < 16; ++column) {
        if (!ended && text[column] == '\0') ended = true;
        lcd->write(static_cast<uint8_t>(ended ? ' ' : text[column]));
    }
}

void setup() {
    Serial.begin(115200);
    delay(300);
    Serial.printf("\n===== %s | TP2 =====\n", GROUP_NAME);
    Wire.begin(21, 22);
    delay(500);
    Serial.println("TP2 - BME680 + LCD on shared I2C");
    for (uint8_t address = 1; address < 127; ++address) {
        Wire.beginTransmission(address);
        if (Wire.endTransmission() == 0) {
            Serial.printf("I2C device: 0x%02X\n", address);
            if (address == 0x27) lcd = &lcd27;
            if (address == 0x3F && lcd == nullptr) lcd = &lcd3f;
        }
    }
    if (lcd != nullptr) {
        lcd->init();
        lcd->backlight();
        lcd->clear();
        Serial.println("LCD ready");
    } else {
        Serial.println("LCD not found at 0x27 or 0x3F");
    }
    sensorReady = bme.begin(0x77);
    if (!sensorReady) sensorReady = bme.begin(0x76);
    if (!sensorReady) {
        Serial.println("BME680 not found at 0x77 or 0x76");
        displayLine(0, "BME680 missing");
        displayLine(1, "Check wiring");
        return;
    }
    bme.setTemperatureOversampling(BME680_OS_8X);
    bme.setHumidityOversampling(BME680_OS_2X);
    bme.setPressureOversampling(BME680_OS_4X);
    bme.setIIRFilterSize(BME680_FILTER_SIZE_3);
    bme.setGasHeater(320, 150);  
    Serial.println("BME680 ready");
}

void loop() {
    if (!sensorReady) {
        delay(2000);
        return;
    }
    Serial.printf("\n[%s] Sensor readings\n", GROUP_NAME);
    if (!bme.performReading()) {
        Serial.println("BME680 reading failed");
        displayLine(0, "Reading failed");
        displayLine(1, "Retrying...");
        delay(2000);
        return;
    }
    char line[32];
    snprintf(line, sizeof(line), "Temp: %.2f C", bme.temperature);
    displayLine(0, line);
    snprintf(line, sizeof(line), "Hum:  %.2f %%", bme.humidity);
    displayLine(1, line);
    Serial.printf("Temperature: %.2f C | %.2f F | Humidity: %.2f %%\n",
                  bme.temperature, bme.temperature * 9.0f / 5.0f + 32.0f,
                  bme.humidity);
    delay(2000);
}
