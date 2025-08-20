#include <Wire.h>
#include <Adafruit_AHTX0.h>
#include <WiFi.h>
#include <PubSubClient.h>

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

// Optional calibration (you can measure and adjust these)
const int dryValue = 8150; // sensor value in completely dry soil
const int wetValue = 1200; // sensor value in fully wet soil (example)

// ---------- Functions ----------
void setup_wifi()
{
    delay(10);
    Serial.println();
    Serial.print("Connecting to ");
    Serial.println(ssid);

    WiFi.begin(ssid, password);

    while (WiFi.status() != WL_CONNECTED)
    {
        delay(500);
        Serial.print(".");
    }

    Serial.println("");
    Serial.println("WiFi connected");
    Serial.println(WiFi.localIP());
}

void reconnect()
{
    // Loop until we're reconnected
    while (!client.connected())
    {
        Serial.print("Attempting MQTT connection...");
        // Attempt to connect
        if (client.connect("ESP32_SoilWeather"))
        {
            Serial.println("connected");
        }
        else
        {
            Serial.print("failed, rc=");
            Serial.print(client.state());
            Serial.println(" try again in 5 seconds");
            delay(5000);
        }
    }
}


void setup()
{
    Serial.begin(115200);

    // Initialize I2C for AHT25
    Wire.begin(8, 9);

    if (!aht.begin())
    {
        Serial.println("WARNING: AHT25 not found!");
        while (1)
            delay(10);
    }
    Serial.println("AHT25 Found!");

    // WiFi
    setup_wifi();
    client.setServer(mqtt_server, 1883);

    pinMode(SOIL_PIN, INPUT);

    // Publish MQTT discovery config for Home Assistant
    client.publish("homeassistant/sensor/garden_soil/config",
                   "{\"name\": \"Garden Soil Moisture\", \"state_topic\": \"home/garden/soil\", \"unit_of_measurement\": \"%\", \"device_class\": \"moisture\"}", true);

    client.publish("homeassistant/sensor/garden_temp/config",
                   "{\"name\": \"Garden Temperature\", \"state_topic\": \"home/garden/temperature\", \"unit_of_measurement\": \"°C\", \"device_class\": \"temperature\"}", true);

    client.publish("homeassistant/sensor/garden_humidity/config",
                   "{\"name\": \"Garden Humidity\", \"state_topic\": \"home/garden/humidity\", \"unit_of_measurement\": \"%\", \"device_class\": \"humidity\"}", true);
}

void loop()
{
    if (!client.connected())
    {
        reconnect();
    }
    client.loop();
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
    
    // Publish to MQTT
    char buffer[50];

    sprintf(buffer, "%d", soilPercent);
    client.publish("home/garden/soil", buffer);

    sprintf(buffer, "%.2f", temp.temperature);
    client.publish("home/garden/temperature", buffer);

    sprintf(buffer, "%.2f", humidity.relative_humidity);
    client.publish("home/garden/humidity", buffer);

    delay(5000); // send every 5 seconds
}
