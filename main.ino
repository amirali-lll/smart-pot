#include <Wire.h>
#include <Adafruit_AHTX0.h>
#include <WiFi.h>
#include <PubSubClient.h>
#ifdef ARDUINO_ARCH_ESP32
#include "esp_sleep.h"
#endif

// ---------- WiFi ----------
const char *ssid = "YOUR WIFI";
const char *password = "YOUR PASSWORD";

// ---------- MQTT ----------
const char *mqtt_server = "192.168.100.10";
WiFiClient espClient;
PubSubClient client(espClient);

// ---------- Sensors ----------
Adafruit_AHTX0 aht;
#define SOIL_PIN 15 // Analog pin for HW-69 soil moisture sensor
// TIP: If analogRead returns inconsistent values while WiFi is ON, move SOIL_PIN to an ADC1 pin (GPIO 32-39) to avoid ADC2/WiFi conflicts on ESP32.

// Optional calibration (you can measure and adjust these)
const int dryValue = 8150; // sensor value in completely dry soil
const int wetValue = 1200; // sensor value in fully wet soil (example)

// ---------- Sleep config ----------
#define uS_TO_S_FACTOR 1000000ULL
#define TIME_TO_SLEEP_S 300 // 5 minutes

bool ahtReady = false;

// ---------- Functions ----------
void setup_wifi()
{
    delay(10);
    Serial.println();
    Serial.print("Connecting to ");
    Serial.println(ssid);

    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);

    unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - start < 20000)
    {
        delay(500);
        Serial.print(".");
    }

    if (WiFi.status() == WL_CONNECTED)
    {
        Serial.println("");
        Serial.println("WiFi connected");
        Serial.println(WiFi.localIP());
    }
    else
    {
        Serial.println("\nWiFi connect timeout. Sleeping to retry later.");
        goToDeepSleep();
    }
}

bool mqttConnect(uint8_t attempts = 10)
{
    uint8_t tries = 0;
    while (!client.connected() && tries < attempts)
    {
        Serial.print("Attempting MQTT connection...");
        if (client.connect("ESP32_SoilWeather"))
        {
            Serial.println("connected");
            return true;
        }
        Serial.print("failed, rc=");
        Serial.print(client.state());
        Serial.println(" retrying in 1s");
        tries++;
        delay(1000);
    }
    return client.connected();
}

void goToDeepSleep()
{
    Serial.println("Preparing for deep sleep...");
    // Grace period to flush serial
    delay(50);

    // Cleanly close MQTT & WiFi for lower consumption
    if (client.connected())
    {
        client.disconnect();
    }
    delay(20);
    WiFi.disconnect(true, true);
    delay(20);
    WiFi.mode(WIFI_OFF);

    esp_sleep_enable_timer_wakeup((uint64_t)TIME_TO_SLEEP_S * uS_TO_S_FACTOR);
    Serial.printf("Going to sleep for %d seconds...\n", TIME_TO_SLEEP_S);
    delay(50);
    esp_deep_sleep_start();
}

void setup()
{
    Serial.begin(115200);

    // Initialize I2C for AHT25
    Wire.begin(8, 9);

    if (!aht.begin())
    {
        Serial.println("WARNING: AHT25 not found! Continuing without it.");
        ahtReady = false;
    }
    else
    {
        ahtReady = true;
        Serial.println("AHT25 Found!");
    }

    // WiFi
    setup_wifi();
    client.setServer(mqtt_server, 1883);

    pinMode(SOIL_PIN, INPUT);

    // Connect to MQTT (bounded retries); if it fails, sleep and try next cycle
    if (!mqttConnect(15))
    {
        Serial.println("MQTT connect failed. Sleeping to retry later.");
        goToDeepSleep();
    }

    // Publish MQTT discovery config for Home Assistant (retained)
    client.publish("homeassistant/sensor/garden_soil/config",
                   "{\"name\": \"Garden Soil Moisture\", \"state_topic\": \"home/garden/soil\", \"unit_of_measurement\": \"%\", \"device_class\": \"moisture\"}", true);

    client.publish("homeassistant/sensor/garden_temp/config",
                   "{\"name\": \"Garden Temperature\", \"state_topic\": \"home/garden/temperature\", \"unit_of_measurement\": \"°C\", \"device_class\": \"temperature\"}", true);

    client.publish("homeassistant/sensor/garden_humidity/config",
                   "{\"name\": \"Garden Humidity\", \"state_topic\": \"home/garden/humidity\", \"unit_of_measurement\": \"%\", \"device_class\": \"humidity\"}", true);

    // --- Read sensors ---
    sensors_event_t humidity, temp;
    if (ahtReady)
    {
        aht.getEvent(&humidity, &temp);
        Serial.print("Temperature: ");
        Serial.print(temp.temperature);
        Serial.println(" °C");

        Serial.print("Air Humidity: ");
        Serial.print(humidity.relative_humidity);
        Serial.println(" %");
    }
    else
    {
        Serial.println("Skipping AHT measurements (sensor not ready).");
    }

    int soilRaw = analogRead(SOIL_PIN); // Raw value (0-4095)
    int soilPercent = map(soilRaw, dryValue, wetValue, 0, 100);
    soilPercent = constrain(soilPercent, 0, 100);

    Serial.print("Soil raw: ");
    Serial.print(soilRaw);
    Serial.print(" -> ");
    Serial.print(soilPercent);
    Serial.println(" %");

    // --- Publish to MQTT ---
    char buffer[50];

    snprintf(buffer, sizeof(buffer), "%d", soilPercent);
    client.publish("home/garden/soil", buffer);

    if (ahtReady)
    {
        snprintf(buffer, sizeof(buffer), "%.2f", temp.temperature);
        client.publish("home/garden/temperature", buffer);

        snprintf(buffer, sizeof(buffer), "%.2f", humidity.relative_humidity);
        client.publish("home/garden/humidity", buffer);
    }

    // Give MQTT stack a brief moment to send packets
    unsigned long t0 = millis();
    while (millis() - t0 < 500)
    {
        client.loop();
        delay(1);
    }

    // Sleep until next cycle
    goToDeepSleep();
}

void loop()
{
    // Not used; device sleeps after setup completes
}
