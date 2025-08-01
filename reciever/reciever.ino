#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <SPIFFS.h>
#include <ArduinoJson.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <HardwareSerial.h>

// --- WiFi Credentials ---
const char* ssid = "A2B";       // Your WiFi SSID
const char* password = "adityaab"; // Your WiFi Password

// --- ThingSpeak Info ---
const char* thingspeakServer = "api.thingspeak.com";
String apiKey = "TGFKUWTJRHCZDTEM"; // Your ThingSpeak API Key

// --- HiveMQ MQTT Info ---
const char* mqtt_server = "4be1c9efcf4447819f8e105ac8f70131.s1.eu.hivemq.cloud";
const int mqtt_port = 8883;
const char* mqtt_topic = "sr_method/vibration/status";
const char* mqtt_client_id = "ESP32_Receiver_Client";
const char* mqtt_username = "sr_method";
const char* mqtt_password = "Such1r_Df";

// --- Serial Connections ---
// Serial to STM32 (ADXL345 data) on UART2
HardwareSerial stmSerial(2);     // RX=16, TX=17
// Serial from Transmitter ESP32 (Sensor data) on UART1
HardwareSerial transmitterSerial(1); // RX=9, TX=10

// --- Global State Variables ---
float current_temperature = 0.0;
int current_water_level = 0;
String current_pump_status = "OFF";
String current_anomaly_status = "NOMINAL";
int current_anomaly_value = 0;

// --- Clients ---
WiFiClientSecure secureClient;
PubSubClient mqttClient(secureClient);
WiFiClient thingClient;
AsyncWebServer server(80);

// --- Timers and Constants ---
unsigned long lastNominalUpdate = 0;
const unsigned long NOMINAL_DELAY = 16000;
unsigned long lastLogTime = 0;
const unsigned long LOG_INTERVAL = 120000; // Log every 2 minutes
const char* LOG_FILE = "/datalog.csv";

// --- WiFi Setup --- (Your original function, works great)
void setup_wifi() {
  Serial.print("Connecting to WiFi");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500); Serial.print(".");
  }
  Serial.println("\nWiFi connected. IP: " + WiFi.localIP().toString());
}

// --- MQTT Connect --- (Your original function)
void connectToMQTT() {
  secureClient.setInsecure();
  mqttClient.setServer(mqtt_server, mqtt_port);
  while (!mqttClient.connected()) {
    Serial.print("Connecting to MQTT...");
    if (mqttClient.connect(mqtt_client_id, mqtt_username, mqtt_password)) {
      Serial.println("connected.");
      mqttClient.publish(mqtt_topic, "ESP32 Receiver online");
    } else {
      Serial.print("failed, rc=");
      Serial.print(mqttClient.state());
      delay(2000);
    }
  }
}

// --- Publish to MQTT --- (Your original function)
void publishToMQTT(String msg) {
  if (!mqttClient.connected()) {
    connectToMQTT();
  }
  bool success = mqttClient.publish(mqtt_topic, msg.c_str());
  if (success)
    Serial.println("MQTT Published: " + msg);
  else
    Serial.println("MQTT publish failed!");
}

// --- Send to ThingSpeak --- (Your original function, with added fields)
void sendToThingSpeak() {
  if (thingClient.connect(thingspeakServer, 80)) {
    String url = "/update?api_key=" + apiKey;
    url += "&field1=" + String(current_anomaly_status == "ANOMALY" ? 1 : 0);
    url += "&field2=" + String(current_anomaly_value);
    url += "&field3=" + String(current_temperature);
    url += "&field4=" + String(current_water_level);
    Serial.println("Sending to ThingSpeak: " + url);
    thingClient.print(String("GET ") + url + " HTTP/1.1\r\n" +
                      "Host: " + thingspeakServer + "\r\n" +
                      "Connection: close\r\n\r\n");
    thingClient.stop();
  } else {
    Serial.println("ThingSpeak connection failed.");
  }
}

// --- Process STM32 Message ---
void handleStmMessage(String message) {
  message.trim();
  Serial.println("Received from STM32: " + message);
  int commaIndex = message.indexOf(',');
  if (commaIndex <= 0) {
      Serial.println("Ignored invalid STM32 message: " + message);
      return;
  }
  
  String statusStr = message.substring(0, commaIndex);
  int value = message.substring(commaIndex + 1).toInt();
  
  current_anomaly_value = value;
  
  if (statusStr == "ANOMALY") {
      current_anomaly_status = "ANOMALY";
      publishToMQTT(message); // Send ANOMALY to MQTT
      sendToThingSpeak(); // Log all data to ThingSpeak on ANOMALY
  } else if (statusStr == "NOMINAL") {
      current_anomaly_status = "NOMINAL";
      // Rate-limit NOMINAL ThingSpeak logging
      if (millis() - lastNominalUpdate > NOMINAL_DELAY) {
          sendToThingSpeak();
          lastNominalUpdate = millis();
      }
  }
}

// --- Process Transmitter ESP32 Message ---
void handleTransmitterMessage(String message) {
  message.trim();
  Serial.println("Received from Transmitter: " + message);

  int tempIndex = message.indexOf("TEMP:");
  int levelIndex = message.indexOf(",LEVEL:");
  int pumpIndex = message.indexOf(",PUMP:");

  if (tempIndex != -1 && levelIndex != -1 && pumpIndex != -1) {
    String tempStr = message.substring(tempIndex + 5, levelIndex);
    String levelStr = message.substring(levelIndex + 7, pumpIndex);
    String pumpStr = message.substring(pumpIndex + 6);
    
    current_temperature = tempStr.toFloat();
    current_water_level = levelStr.toInt();
    current_pump_status = pumpStr;
  } else {
    Serial.println("Ignored invalid Transmitter message format.");
  }
}

// --- File System Functions ---
void appendFile(fs::FS &fs, const char * path, const char * message) {
    File file = fs.open(path, FILE_APPEND);
    if(!file) {
        Serial.println("Failed to open file for appending");
        return;
    }
    file.println(message);
    file.close();
}

void logDataToCsv() {
    String timestamp = String(millis() / 1000); // Simple timestamp
    String logEntry = timestamp + "," + String(current_temperature) + "," + String(current_water_level) + "," +
                      current_pump_status + "," + current_anomaly_status + "," + String(current_anomaly_value);

    appendFile(SPIFFS, LOG_FILE, logEntry.c_str());
    Serial.println("Logged data to CSV: " + logEntry);
}

// --- Web Server Request Handlers ---
void setupWebServer() {
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(SPIFFS, "/index.html", "text/html");
    });
    server.on("/style.css", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(SPIFFS, "/style.css", "text/css");
    });
    server.on("/script.js", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(SPIFFS, "/script.js", "text/javascript");
    });

    server.on("/data", HTTP_GET, [](AsyncWebServerRequest *request){
        JsonDocument doc;
        doc["temperature"] = current_temperature;
        doc["waterLevel"] = current_water_level;
        doc["pumpStatus"] = current_pump_status;
        doc["anomalyStatus"] = current_anomaly_status;
        doc["anomalyValue"] = current_anomaly_value;
        String json;
        serializeJson(doc, json);
        request->send(200, "application/json", json);
    });
    
    server.on("/log", HTTP_GET, [](AsyncWebServerRequest *request) {
        File file = SPIFFS.open(LOG_FILE, "r");
        if (!file) {
            request->send(200, "application/json", "[]");
            return;
        }

        JsonArray doc = JsonDocument().to<JsonArray>();
        // Read last 10 lines (simple approach)
        String lines[10];
        int lineCount = 0;
        while(file.available()){
            lines[lineCount % 10] = file.readStringUntil('\n');
            lineCount++;
        }
        file.close();

        for(int i = 0; i < 10 && i < lineCount; i++){
          String line = lines[(lineCount - i - 1) % 10];
          line.trim();
          if(line.length() == 0) continue;
          
          JsonObject obj = doc.add<JsonObject>();
          char lineBuf[256];
          line.toCharArray(lineBuf, sizeof(lineBuf));

          char *p = strtok(lineBuf, ",");
          if(p) obj["timestamp"] = p;
          p = strtok(NULL, ",");
          if(p) obj["temperature"] = p;
          p = strtok(NULL, ",");
          if(p) obj["waterLevel"] = p;
          p = strtok(NULL, ",");
          if(p) obj["pumpStatus"] = p;
          p = strtok(NULL, ",");
          if(p) obj["anomalyStatus"] = p;
          p = strtok(NULL, ",");
          if(p) obj["anomalyValue"] = p;
        }
        
        String json;
        serializeJson(doc, json);
        request->send(200, "application/json", json);
    });

    server.on("/download", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(SPIFFS, LOG_FILE, "text/csv", true);
    });

    server.on("/clear", HTTP_POST, [](AsyncWebServerRequest *request){
        SPIFFS.remove(LOG_FILE);
        // Create a new empty file with header
        File file = SPIFFS.open(LOG_FILE, FILE_WRITE);
        file.println("Timestamp,Temp,Level,Pump,Anomaly,Value");
        file.close();
        request->send(200, "text/plain", "Log cleared");
    });
    
    server.begin();
}

void setup() {
    Serial.begin(115200);

    // Start UART for STM32
    stmSerial.begin(9600, SERIAL_8N1, 16, 17);
    // Start UART for Transmitter ESP32
    transmitterSerial.begin(9600, SERIAL_8N1, 9, 10);
    
    // Initialize SPIFFS
    if(!SPIFFS.begin(true)){
        Serial.println("An Error has occurred while mounting SPIFFS");
        return;
    }

    // Create log file with header if it doesn't exist
    if (!SPIFFS.exists(LOG_FILE)) {
      File file = SPIFFS.open(LOG_FILE, FILE_WRITE);
      if (file) {
        file.println("Timestamp,Temp,Level,Pump,Anomaly,Value");
        file.close();
      }
    }

    setup_wifi();
    connectToMQTT();
    setupWebServer();

    Serial.println("Receiver setup complete. Web server started.");
    Serial.print("Access at http://");
    Serial.println(WiFi.localIP());
}

void loop() {
  if (!mqttClient.connected()) {
    connectToMQTT();
  }
  mqttClient.loop();

  // Check for data from STM32
  if (stmSerial.available()) {
      static String stmLine = "";
      char c = stmSerial.read();
      if (c == '\n') {
          handleStmMessage(stmLine);
          stmLine = "";
      } else {
          stmLine += c;
      }
  }

  // Check for data from Transmitter ESP32
  if (transmitterSerial.available()) {
      static String transmitterLine = "";
      char c = transmitterSerial.read();
      if (c == '\n') {
          handleTransmitterMessage(transmitterLine);
          transmitterLine = "";
      } else {
          transmitterLine += c;
      }
  }

  // Log data periodically
  if (millis() - lastLogTime > LOG_INTERVAL) {
      logDataToCsv();
      lastLogTime = millis();
  }
}