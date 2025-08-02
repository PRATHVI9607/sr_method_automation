// =================================================================
//                 AQUARIUM AUTOMATION - RECEIVER
// =================================================================
// This code is the central hub for the aquarium monitor. It:
// - Listens for sensor data from a transmitter ESP32.
// - Listens for anomaly data from an STM32 board.
// - Hosts a web server to display live data.
// - Logs all data to a local CSV file.
// - Sends data and alerts to cloud services (ThingSpeak, IFTTT).
// -----------------------------------------------------------------

// --- LIBRARIES ---
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <SPIFFS.h>
#include <ArduinoJson.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <HardwareSerial.h>
#include <HTTPClient.h>

// --- CONFIGURATION: NETWORK & SERVICES ---
const char* WIFI_SSID       = "HACHIMAN_PRATHVI";
const char* WIFI_PASSWORD   = "prathvi9607";

const char* THINGSPEAK_API_KEY = "TGFKUWTJRHCZDTEM";

const char* MQTT_SERVER     = "4be1c9efcf4447819f8e105ac8f70131.s1.eu.hivemq.cloud";
const int   MQTT_PORT       = 8883;
const char* MQTT_TOPIC      = "sr_method/vibration/status";
const char* MQTT_CLIENT_ID  = "ESP32_Receiver_Client";
const char* MQTT_USERNAME   = "sr_method";
const char* MQTT_PASSWORD   = "Such1r_Df";

const char* IFTTT_API_KEY   = "YOUR_IFTTT_KEY_HERE";
const char* IFTTT_EVENT     = "aquarium_anomaly";

// --- CONFIGURATION: HARDWARE & TIMING ---
#define STM32_RX_PIN        16
#define STM32_TX_PIN        17 // Not used for receiving, but good practice to define
#define TRANSMITTER_RX_PIN  21
#define TRANSMITTER_TX_PIN  22

const unsigned long LOG_INTERVAL_MS = 120000; // 2 minutes
const char* LOG_FILE_PATH = "/datalog.csv";

// --- GLOBAL STATE VARIABLES ---
// These variables hold the latest data from all sensors.
float   g_temperature = 0.0;
int     g_waterLevel  = 0;
String  g_pumpStatus  = "OFF";
String  g_anomalyStatus = "NOMINAL";
int     g_anomalyValue  = 0;
bool    g_notificationSent = false;

// --- GLOBAL OBJECTS ---
WiFiClientSecure g_secureClient;
PubSubClient     g_mqttClient(g_secureClient);
AsyncWebServer   g_server(80);
HardwareSerial   g_stmSerial(2);
HardwareSerial   g_transmitterSerial(1);
unsigned long    g_lastLogTime = 0;

// =================================================================
//                  FUNCTION PROTOTYPES
// =================================================================
// --- SETUP FUNCTIONS ---
void setupCommunications();
void setupNetworking();
void setupFileSystem();
void setupWebServer();

// --- NETWORK FUNCTIONS ---
void connectWiFi();
void connectMQTT();
void sendToThingSpeak();
void triggerIFTTT(const String& status, int value);

// --- LOGIC & HANDLER FUNCTIONS ---
void handleStmMessage(const String& message);
void handleTransmitterMessage(const String& message);
void listFiles();
void logData();
void checkSerialPorts();


// =================================================================
//                   PRIMARY SETUP AND LOOP
// =================================================================
void setup() {
    Serial.begin(115200);
    Serial.println("\n\n--- Booting Aquarium Monitor Receiver ---");

    setupCommunications();
    setupNetworking();
    setupFileSystem();
    setupWebServer();

    Serial.println("\n--- Setup Complete: System is now running. ---");
}

void loop() {
    // Keep MQTT connection alive
    if (!g_mqttClient.connected()) {
        connectMQTT();
    }
    g_mqttClient.loop();

    // Check for incoming data on serial ports
    checkSerialPorts();

    // Periodically log data to the CSV file
    if (millis() - g_lastLogTime > LOG_INTERVAL_MS) {
        logData();
        g_lastLogTime = millis();
    }
}


// =================================================================
//                       SETUP FUNCTIONS
// =================================================================
void setupCommunications() {
    g_stmSerial.begin(9600, SERIAL_8N1, STM32_RX_PIN, STM32_TX_PIN);
    Serial.println("UART for STM32 initialized.");
    g_transmitterSerial.begin(9600, SERIAL_8N1, TRANSMITTER_RX_PIN, TRANSMITTER_TX_PIN);
    Serial.println("UART for Transmitter ESP32 initialized.");
}

void setupNetworking() {
    connectWiFi();
    connectMQTT();
}

void setupFileSystem() {
    if (!SPIFFS.begin(true)) {
        Serial.println("[FATAL] Failed to mount SPIFFS. Halting.");
        while (1) { delay(1000); }
    }
    Serial.println("File System mounted.");
    listFiles(); // List files for debugging purposes

    // Create log file with header if it doesn't exist
    if (!SPIFFS.exists(LOG_FILE_PATH)) {
        File file = SPIFFS.open(LOG_FILE_PATH, FILE_WRITE);
        if (file) {
            file.println("Timestamp,Temp,Level,Pump,Anomaly,Value");
            file.close();
            Serial.println("Created new log file.");
        }
    }
}


// =================================================================
//                       NETWORK FUNCTIONS
// =================================================================
void connectWiFi() {
    Serial.print("Connecting to WiFi...");
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    WiFi.setSleep(false);

    unsigned long startTime = millis();
    while (WiFi.status() != WL_CONNECTED) {
        if (millis() - startTime > 20000) { // 20-second timeout
            Serial.println("\n[FATAL] WiFi connection failed. Please check credentials. Rebooting.");
            delay(1000);
            ESP.restart();
        }
        Serial.print(".");
        delay(500);
    }
    Serial.println("\nWiFi connected. IP Address: " + WiFi.localIP().toString());
}

void connectMQTT() {
    g_secureClient.setInsecure(); // Required for HiveMQ Cloud with this library
    g_mqttClient.setServer(MQTT_SERVER, MQTT_PORT);

    if (g_mqttClient.connect(MQTT_CLIENT_ID, MQTT_USERNAME, MQTT_PASSWORD)) {
        Serial.println("MQTT connected successfully.");
        g_mqttClient.publish(MQTT_TOPIC, "ESP32 Receiver Online");
    } else {
        Serial.println("[Warning] MQTT connection failed. Will retry in the background.");
    }
}

void sendToThingSpeak() {
    if (WiFi.status() != WL_CONNECTED) return;
    
    WiFiClient client;
    if (client.connect("api.thingspeak.com", 80)) {
        String url = "/update?api_key=" + String(THINGSPEAK_API_KEY);
        url += "&field1=" + String(g_anomalyStatus == "ANOMALY" ? 1 : 0);
        url += "&field2=" + String(g_anomalyValue);
        url += "&field3=" + String(g_temperature);
        url += "&field4=" + String(g_waterLevel);
        
        client.print("GET " + url + " HTTP/1.1\r\n");
        client.print("Host: api.thingspeak.com\r\n");
        client.print("Connection: close\r\n\r\n");
        client.stop();
        Serial.println("ThingSpeak data sent.");
    }
}

void triggerIFTTT(const String& status, int value) {
    if (WiFi.status() != WL_CONNECTED) return;
    
    HTTPClient http;
    String url = "https://maker.ifttt.com/trigger/" + String(IFTTT_EVENT) + "/with/key/" + String(IFTTT_API_KEY);
    http.begin(url);
    http.addHeader("Content-Type", "application/json");

    JsonDocument doc;
    doc["value1"] = status;
    doc["value2"] = value;
    String payload;
    serializeJson(doc, payload);

    if (http.POST(payload) == HTTP_CODE_OK) {
        Serial.println("IFTTT alert triggered successfully.");
    } else {
        Serial.println("[Error] IFTTT alert failed.");
    }
    http.end();
}


// =================================================================
//                  LOGIC & HANDLER FUNCTIONS
// =================================================================

void checkSerialPorts() {
    if (g_stmSerial.available()) {
        String message = g_stmSerial.readStringUntil('\n');
        handleStmMessage(message);
    }
    if (g_transmitterSerial.available()) {
        String message = g_transmitterSerial.readStringUntil('\n');
        handleTransmitterMessage(message);
    }
}

void handleStmMessage(const String& message) {
    String msg = message;
    msg.trim();
    
    int comma = msg.indexOf(',');
    if (comma == -1) return; // Invalid format

    g_anomalyStatus = msg.substring(0, comma);
    g_anomalyValue = msg.substring(comma + 1).toInt();

    if (g_anomalyStatus == "ANOMALY") {
        if (!g_notificationSent) {
            Serial.println(">>> ANOMALY DETECTED! Sending alert. <<<");
            triggerIFTTT(g_anomalyStatus, g_anomalyValue);
            g_notificationSent = true;
        }
    } else { // NOMINAL
        if (g_notificationSent) {
            Serial.println(">>> System returned to NOMINAL. <<<");
            g_notificationSent = false;
        }
    }
    
    // Always send data when anomaly is detected or things have changed
    if (g_anomalyStatus == "ANOMALY") {
      if (g_mqttClient.connected()) g_mqttClient.publish(MQTT_TOPIC, msg.c_str());
      sendToThingSpeak();
    }
}

void handleTransmitterMessage(const String& message) {
    String msg = message;
    msg.trim();
    
    // More robust parsing
    int tempStart = msg.indexOf("TEMP:") + 5;
    int levelStart = msg.indexOf("LEVEL:") + 6;
    int pumpStart = msg.indexOf("PUMP:") + 5;

    if (tempStart > 4 && levelStart > 5 && pumpStart > 4) {
        g_temperature = msg.substring(tempStart, msg.indexOf(",LEVEL:")).toFloat();
        g_waterLevel = msg.substring(levelStart, msg.indexOf(",PUMP:")).toInt();
        g_pumpStatus = msg.substring(pumpStart);
    }
}

void logData() {
    File file = SPIFFS.open(LOG_FILE_PATH, FILE_APPEND);
    if (file) {
        String entry = String(millis()) + "," + String(g_temperature) + "," +
                       String(g_waterLevel) + "," + g_pumpStatus + "," +
                       g_anomalyStatus + "," + String(g_anomalyValue);
        file.println(entry);
        file.close();
        Serial.println("Data logged to CSV.");
    }
}

void listFiles() {
    Serial.println("\n--- Listing Files in SPIFFS ---");
    File root = SPIFFS.open("/");
    File file = root.openNextFile();
    while(file){
        Serial.printf("  FILE: %s, SIZE: %d bytes\n", file.name(), file.size());
        file = root.openNextFile();
    }
    if (!SPIFFS.exists("/index.html")) {
      Serial.println("[CRITICAL ERROR] /index.html is missing!");
    }
    Serial.println("--- File List Complete ---\n");
}


// =================================================================
//                       WEB SERVER SETUP
// =================================================================

void setupWebServer() {
    // Handler for live data
    g_server.on("/data", HTTP_GET, [](AsyncWebServerRequest *request){
        JsonDocument doc;
        doc["temperature"] = g_temperature;
        doc["waterLevel"] = g_waterLevel;
        doc["pumpStatus"] = g_pumpStatus;
        doc["anomalyStatus"] = g_anomalyStatus;
        doc["anomalyValue"] = g_anomalyValue;
        String json;
        serializeJson(doc, json);
        request->send(200, "application/json", json);
    });

    // Handler to download the full log file
    g_server.on("/download", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(SPIFFS, LOG_FILE_PATH, "text/csv", true);
    });

    // Handler to clear the log file
    g_server.on("/clear", HTTP_POST, [](AsyncWebServerRequest *request){
        SPIFFS.remove(LOG_FILE_PATH);
        // Recreate the file with a header
        File file = SPIFFS.open(LOG_FILE_PATH, FILE_WRITE);
        if (file) { file.println("Timestamp,Temp,Level,Pump,Anomaly,Value"); file.close(); }
        request->send(200, "text/plain", "Log Cleared");
    });
    
    // This serves the main web page and all associated files (CSS, JS)
    g_server.serveStatic("/", SPIFFS, "/").setDefaultFile("index.html");

    // Handler for "Page Not Found"
    g_server.onNotFound([](AsyncWebServerRequest *request){
        request->send(404, "text/plain", "Error: Not Found");
    });

    g_server.begin();
    Serial.println("Web Server has started.");
}
