#include "../../group.h"
#include "secrets.h"
#include "vps_ca.h"
#include "tls_roots.h"
#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_BME680.h>
#include <LiquidCrystal_I2C.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <time.h>

Adafruit_BME680 bme;
LiquidCrystal_I2C lcd(0x27, 16, 2);
WiFiClientSecure vpsSocket, aioSocket;
PubSubClient vps(vpsSocket), aio(aioSocket);
bool sensorReady = false, lcdReady = false, haveReading = false;
float temperatureC = 0, humidity = 0;
uint32_t lastReading = 0, lastPublish = 0, lastVpsConnect = 0, lastAioConnect = 0;

void displayLine(uint8_t row, const char *text)
{
    if (!lcdReady)
        return;
    lcd.setCursor(0, row);
    bool ended = false;
    for (uint8_t i = 0; i < 16; ++i)
    {
        if (!ended && text[i] == '\0')
            ended = true;
        lcd.write(static_cast<uint8_t>(ended ? ' ' : text[i]));
    }
}

void setup()
{
    Serial.begin(115200);
    delay(500);
    Serial.printf("\n===== %s | TP2 MQTT =====\n", GROUP_NAME);
    Serial.println("BME680 -> ESP32 -> Mosquitto VPS + Adafruit IO");
    Wire.begin(21, 22);
    for (uint8_t address = 1; address < 127; ++address)
    {
        Wire.beginTransmission(address);
        if (Wire.endTransmission() == 0)
            Serial.printf("I2C device: 0x%02X\n", address);
    }
    Wire.beginTransmission(0x27);
    lcdReady = Wire.endTransmission() == 0;
    if (lcdReady)
    {
        lcd.init();
        lcd.backlight();
        lcd.clear();
    }
    sensorReady = bme.begin(0x77);
    if (!sensorReady)
        sensorReady = bme.begin(0x76);
    if (!sensorReady)
    {
        Serial.println("BME680 not found; check wiring with USB unplugged.");
        displayLine(0, "BME680 missing");
        return;
    }
    bme.setTemperatureOversampling(BME680_OS_8X);
    bme.setHumidityOversampling(BME680_OS_2X);
    bme.setPressureOversampling(BME680_OS_4X);
    bme.setIIRFilterSize(BME680_FILTER_SIZE_3);
    bme.setGasHeater(320, 150);
    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(true);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    configTime(0, 0, "pool.ntp.org", "time.google.com");
    vpsSocket.setCACert(VPS_CA);
    aioSocket.setCACert(TLS_ROOTS);
    vpsSocket.setHandshakeTimeout(15);
    aioSocket.setHandshakeTimeout(15);
    vps.setServer(VPS_HOST, 8883);
    aio.setServer("io.adafruit.com", 8883);
    vps.setBufferSize(512);
    aio.setBufferSize(512);
    vps.setKeepAlive(60);
    aio.setKeepAlive(60);
    vps.setSocketTimeout(5);
    aio.setSocketTimeout(5);
    Serial.println("Wi-Fi connecting; MQTT uses TLS on port 8883.");
    if (!strlen(AIO_USERNAME))
        Serial.println("Adafruit IO not configured yet.");
}

void connectBroker(PubSubClient &broker, uint32_t &lastAttempt,
                   const char *label, const char *username, const char *password)
{
    if (broker.connected() || millis() - lastAttempt < 15000)
        return;
    lastAttempt = millis();
    String clientId = String("group4-") + label + "-" + String(static_cast<uint32_t>(ESP.getEfuseMac()), HEX);
    const bool connected = broker.connect(clientId.c_str(), username, password);
    Serial.printf("[%s] %s MQTT: %s (state %d)\n", GROUP_NAME, label,
                  connected ? "CONNECTED" : "retry pending", broker.state());
}

void loop()
{
    if (!sensorReady)
    {
        delay(1000);
        return;
    }
    if (!haveReading || millis() - lastReading >= 2000)
    {
        lastReading = millis();
        if (bme.performReading())
        {
            temperatureC = bme.temperature;
            humidity = bme.humidity;
            haveReading = true;
            char line[32];
            snprintf(line, sizeof(line), "Temp: %.2f C", temperatureC);
            displayLine(0, line);
            snprintf(line, sizeof(line), "Hum:  %.2f %%", humidity);
            displayLine(1, line);
        }
        else
        {
            Serial.printf("[%s] Sensor read FAILED\n", GROUP_NAME);
        }
    }
    const bool networkReady = WiFi.status() == WL_CONNECTED && time(nullptr) > 1700000000;
    if (networkReady)
    {
        connectBroker(vps, lastVpsConnect, "VPS", VPS_USERNAME, VPS_PASSWORD);
        if (strlen(AIO_USERNAME))
            connectBroker(aio, lastAioConnect, "Adafruit", AIO_USERNAME, AIO_KEY);
        vps.loop();
        if (strlen(AIO_USERNAME))
            aio.loop();
    }
    if (haveReading && millis() - lastPublish >= 10000)
    {
        lastPublish = millis();
        Serial.printf("\n===== %s | TP2 MQTT | uptime %lus =====\n", GROUP_NAME,
                      static_cast<unsigned long>(millis() / 1000));
        Serial.printf("Temperature: %.2f C | %.2f F | Humidity: %.2f %%\n",
                      temperatureC, temperatureC * 9.0f / 5.0f + 32.0f, humidity);
        char payload[160];
        snprintf(payload, sizeof(payload), "{\"temperature_c\":%.2f,\"humidity_pct\":%.2f,\"uptime_s\":%lu}",
                 temperatureC, humidity, static_cast<unsigned long>(millis() / 1000));
        const bool sent = vps.connected() && vps.publish("iot/tp2/readings", payload, false);
        Serial.printf("Mosquitto VPS | iot/tp2/readings | %s\n", sent ? "PUBLISHED" : "WAITING / retry next sample");
        if (strlen(AIO_USERNAME))
        {
            char value[24];
            String topic = String(AIO_USERNAME) + "/feeds/temperature";
            snprintf(value, sizeof(value), "%.2f", temperatureC);
            const bool sentT = aio.connected() && aio.publish(topic.c_str(), value);
            topic = String(AIO_USERNAME) + "/feeds/humidity";
            snprintf(value, sizeof(value), "%.2f", humidity);
            const bool sentH = aio.connected() && aio.publish(topic.c_str(), value);
            Serial.printf("Adafruit IO | temperature: %s | humidity: %s\n", sentT ? "PUBLISHED" : "WAITING", sentH ? "PUBLISHED" : "WAITING");
        }
        else
            Serial.println("Adafruit IO | pending account configuration");
        if (!networkReady)
            Serial.println(WiFi.status() == WL_CONNECTED ? "Waiting for network time" : "Waiting for Wi-Fi hotspot");
    }
    delay(5);
}
