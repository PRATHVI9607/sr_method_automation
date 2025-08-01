#include <OneWire.h>
#include <DallasTemperature.h>
#include <HardwareSerial.h>

// --- Pin Definitions ---
// DS18B20 Temperature Sensor
#define ONE_WIRE_BUS 4

// Analog Water Level Sensor
#define WATER_LEVEL_PIN 34

// 5V Water Pump Relay
#define PUMP_RELAY_PIN 5

// --- Water Level Configuration ---
// IMPORTANT: Calibrate these values by reading the sensor's output when dry and when fully submerged.
#define WATER_LEVEL_EMPTY 1800 // ADC value when the sensor is dry (example value)
#define WATER_LEVEL_FULL 900   // ADC value when the sensor is fully submerged (example value)
#define REFILL_THRESHOLD_PERCENT 20 // Start pump when level is below this
#define REFILL_STOP_PERCENT 95      // Stop pump when level is above this

// --- Setup Dallas Temperature Sensor ---
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);

// --- Serial connection to Receiver ESP32 (UART1) ---
// We will use UART1 for this communication.
HardwareSerial ReceiverSerial(1);

void setup() {
  // Start the primary serial for debugging
  Serial.begin(115200);

  // Start the serial for communication with the receiver
  // RX on GPIO 9, TX on GPIO 10
  ReceiverSerial.begin(9600, SERIAL_8N1, 9, 10);

  // Initialize the temperature sensor
  sensors.begin();

  // Set the pump relay pin as an output
  pinMode(PUMP_RELAY_PIN, OUTPUT);
  // Ensure the pump is off at the start
  digitalWrite(PUMP_RELAY_PIN, LOW);
  
  Serial.println("Transmitter ESP32 Initialized.");
  Serial.println("Calibrate your water level sensor. Current raw reading:");
  Serial.println(analogRead(WATER_LEVEL_PIN));
}

void loop() {
  // --- Read Temperature ---
  sensors.requestTemperatures();
  float temperatureC = sensors.getTempCByIndex(0);
  if (temperatureC == DEVICE_DISCONNECTED_C) {
    Serial.println("Error: Could not read temperature data");
    temperatureC = -1; // Use an error value
  }

  // --- Read Water Level ---
  int waterLevelRaw = analogRead(WATER_LEVEL_PIN);
  int waterLevelPercent = map(waterLevelRaw, WATER_LEVEL_EMPTY, WATER_LEVEL_FULL, 0, 100);
  waterLevelPercent = constrain(waterLevelPercent, 0, 100); // Keep it within 0-100%

  // --- Control Pump ---
  static bool isPumping = false;
  if (waterLevelPercent <= REFILL_THRESHOLD_PERCENT && !isPumping) {
    isPumping = true;
    digitalWrite(PUMP_RELAY_PIN, HIGH); // Turn the pump ON
  } else if (waterLevelPercent >= REFILL_STOP_PERCENT && isPumping) {
    isPumping = false;
    digitalWrite(PUMP_RELAY_PIN, LOW); // Turn the pump OFF
  }
  String pumpStatus = isPumping ? "ON" : "OFF";
  
  // --- Create Data String ---
  // Format: "TEMP:25.50,LEVEL:85,PUMP:ON"
  String dataString = "TEMP:" + String(temperatureC, 2) + ",";
  dataString += "LEVEL:" + String(waterLevelPercent) + ",";
  dataString += "PUMP:" + pumpStatus;

  // --- Send Data to Receiver ---
  ReceiverSerial.println(dataString);

  // --- Print to Debug Serial ---
  Serial.print("Raw Level: " + String(waterLevelRaw));
  Serial.println(" | Sent: " + dataString);

  // --- Delay before next reading ---
  delay(5000); // Send data every 5 seconds
}