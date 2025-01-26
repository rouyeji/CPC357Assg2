#include <PubSubClient.h>
#include <WiFi.h>
#include <ESP32Servo.h>
#include <Arduino_JSON.h>

// WiFi credentials
const char* WIFI_SSID = "Error";
const char* WIFI_PASSWORD = "wifinotfound_";

// GCP VM MQTT Broker details
const char* MQTT_SERVER = "34.55.234.96";  // Replace with your GCP VM IP or domain
const int MQTT_PORT = 1883;                // Default MQTT port

// MQTT credentials
const char* MQTT_USER = "ESP32User";       // MQTT username
const char* MQTT_PASSWORD = "cpc357";     // MQTT password

// MQTT topics
const char* TOPIC_TELEMETRY = "garbage/telemetry"; // Aggregated data topic
const char* TOPIC_CONTROL_SERVO = "garbage/servo/control";

// Define Pins of sensors and actuators
const int pirPin = 14;       // PIR sensor pin
const int servoPin = 17;     // Servo motor pin
const int trigPin = 26;      // Ultrasonic sensor trig pin
const int echoPin = 25;      // Ultrasonic sensor echo pin

// Initialization variables
int pirState = LOW;
long duration;
float distance;
bool servoEnabled = true;  // Tracks servo motor state
bool isLidOpen = false;    // Tracks lid state
unsigned long lidOpenTime = 0; // Timer for lid closure

Servo servo;
WiFiClient espClient;
PubSubClient mqttClient(espClient);

// Function to connect to WiFi
void setup_wifi() {
  delay(10);
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(WIFI_SSID);

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println();
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

// Function to connect to MQTT broker
void connectToMQTT() {
  while (!mqttClient.connected()) {
    Serial.print("Connecting to MQTT broker...");
    // Use MQTT authentication credentials
    if (mqttClient.connect("ESP32Client", MQTT_USER, MQTT_PASSWORD)) {
      Serial.println("connected");
      mqttClient.subscribe(TOPIC_CONTROL_SERVO);
    } else {
      Serial.print("failed with state ");
      Serial.println(mqttClient.state());
      delay(2000);
    }
  }
}

// Ultrasonic sensor function
float readUltrasonic() {
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);

  duration = pulseIn(echoPin, HIGH);
  distance = duration * 0.034 / 2;
  return distance;
}

// Function to publish aggregated telemetry data
void publishTelemetryData() {
  // Read ultrasonic data
  float distance = readUltrasonic();

  // Determine lid status
  String lidStatus = isLidOpen ? "open" : "closed";

  // Determine waste level based on distance
  String wasteLevel;
  if (distance > 15) {
    wasteLevel = "green"; // Green: Waste level < 50%
  } else if (distance >= 9 && distance <= 15) {
    wasteLevel = "yellow"; // Yellow: Waste level between 50% and 70%
  } else {
    wasteLevel = "red"; // Red: Waste level >= 70%
  }

  // Create JSON object for telemetry data
  String telemetryData = "{";
  telemetryData += "\"distance\": " + String(distance) + ", ";
  telemetryData += "\"motion\": " + String(pirState == HIGH ? "true" : "false") + ", ";
  telemetryData += "\"lidStatus\": \"" + lidStatus + "\", ";
  telemetryData += "\"wasteLevel\": \"" + wasteLevel + "\", "; // Add waste level
  telemetryData += "\"timestamp\": " + String(millis());
  telemetryData += "}";

  // Publish to MQTT topic
  mqttClient.publish(TOPIC_TELEMETRY, telemetryData.c_str());
  Serial.println("Published telemetry data: " + telemetryData);
}

// Callback function for MQTT messages
void mqttCallback(char* topic, byte* payload, unsigned int length) {
  payload[length] = '\0'; // Null-terminate the payload
  String message = String((char*)payload);

  Serial.print("Message received on topic ");
  Serial.print(topic);
  Serial.print(": ");
  Serial.println(message);

  // Handle servo control via MQTT
  if (String(topic) == TOPIC_CONTROL_SERVO) {
    if (message == "ON") {
      Serial.println("Servo enabled");
      servoEnabled = true;
    } else if (message == "OFF") {
      Serial.println("Servo disabled");
      servoEnabled = false;
      servo.write(0); // Close the lid if disabled
    }
  }
}

void setup() {
  Serial.begin(9600);
  Serial.println("Initializing system...");

  setup_wifi();
  mqttClient.setServer(MQTT_SERVER, MQTT_PORT);  // Configure MQTT client
  mqttClient.setCallback(mqttCallback);         // Set MQTT callback function

  pinMode(pirPin, INPUT);
  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);

  servo.attach(servoPin);
  servo.write(0);  // Initialize servo position to closed

  Serial.println("System Initialized. Waiting for motion...");

  Serial.print("ESP32 IP Address: ");
  Serial.println(WiFi.localIP());
}

void loop() {
  // Check MQTT connection
  if (!mqttClient.connected()) {
    connectToMQTT();
  }
  mqttClient.loop();

  // Read PIR sensor state
  int motionDetected = digitalRead(pirPin);

  if (motionDetected == HIGH && pirState == LOW) {
    Serial.println("Motion detected!");
    pirState = HIGH;

    // Control servo motor if enabled
    if (servoEnabled && !isLidOpen) {
      Serial.println("Opening lid...");
      servo.write(180); // Open the lid
      lidOpenTime = millis(); // Record the lid open time
      isLidOpen = true;
    }

    // Publish aggregated telemetry data
    publishTelemetryData();
  } else if (motionDetected == LOW && pirState == HIGH) {
    pirState = LOW;
    Serial.println("No motion detected");

    // Publish aggregated telemetry data
    publishTelemetryData();
  }

  // Automatically close the lid after 8 seconds
  if (isLidOpen && millis() - lidOpenTime >= 8000) {
    Serial.println("Closing lid...");
    servo.write(0); // Close the lid
    isLidOpen = false;

    // Publish aggregated telemetry data after closing the lid
    publishTelemetryData();
  }
}
