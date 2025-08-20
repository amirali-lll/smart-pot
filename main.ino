#include <Wire.h>
#include <Adafruit_AHTX0.h>

Adafruit_AHTX0 aht;

#define SOIL_PIN 15 // AO pin of HW-69 connected to GPIO15

// Optional calibration (you can measure and adjust these)
const int dryValue = 8150; // sensor value in completely dry soil
const int wetValue = 1200; // sensor value in fully wet soil (example)

void setup()
{
    Serial.begin(115200);
    Serial.println("AHT25 + HW-69 Soil Moisture Sensor Test");

    // Initialize I2C for AHT25
    Wire.begin(8, 9);

    if (!aht.begin())
    {
        Serial.println("Could not find AHT25! Check wiring.");
        while (1)
            delay(10);
    }
    Serial.println("AHT25 Found!");

    pinMode(SOIL_PIN, INPUT);
}

void loop()
{
    // --- Read AHT25 (Temperature + Air Humidity) ---
    sensors_event_t humidity, temp;
    aht.getEvent(&humidity, &temp);

    Serial.print("Temperature: ");
    Serial.print(temp.temperature);
    Serial.println(" °C");

    Serial.print("Air Humidity: ");
    Serial.print(humidity.relative_humidity);
    Serial.println(" %");

    // --- Read HW-69 Soil Moisture Sensor ---
    int soilRaw = analogRead(SOIL_PIN); // Raw value (0-4095)

    // Map raw value to percentage: Wet = 100%, Dry = 0%
    int soilPercent = map(soilRaw, dryValue, wetValue, 0, 100);
    soilPercent = constrain(soilPercent, 0, 100); // keep in valid range

    Serial.print("Soil Moisture (raw): ");
    Serial.println(soilRaw);

    Serial.print("Soil Humidity (%): ");
    Serial.print(soilPercent);
    Serial.println(" %");

    Serial.println("----------------------");
    delay(2000);
}
