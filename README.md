# SR Method Automation & Live Monitoring Project

This project provides a complete solution for monitoring and automating key aspects of an sr method, including temperature, water level, and pump control, with real-time anomaly detection and a professional web interface.

## Features

-   **Live Data Monitoring**: A web server hosted on an ESP32 displays live data.
-   **Automated Water Top-up**: Automatically controls a pump to maintain water levels.
-   **Anomaly Detection**: An STM32 with an accelerometer detects unusual vibrations (like a pump running dry or equipment malfunction).
-   **Cloud Integration**: Pushes data to ThingSpeak for historical analysis and sends anomaly alerts to an MQTT broker.
-   **Local Data Logging**: Records all sensor data to a CSV file on the ESP32's internal storage (SPIFFS).
-   **Professional UI**: A clean, responsive web interface with live charts, data cards, and a downloadable log.

## System Architecture

The project consists of three main components:

1.  **STM32 (Vibration Sensor)**:
    -   Connected to an ADXL345 accelerometer.
    -   Analyzes vibration data.
    -   Sends "NOMINAL" or "ANOMALY" status to the Receiver ESP32 via UART.

2.  **Transmitter ESP32 (Sensor Hub)**:
    -   Connected to a DS18B20 temperature sensor and an analog water level sensor.
    -   Controls a 5V water pump via a relay.
    -   Sends formatted sensor data to the Receiver ESP32 via UART.

3.  **Receiver ESP32 (Main Controller & Web Server)**:
    -   Receives data from both the STM32 and the Transmitter ESP32 on two separate UARTs.
    -   Hosts the live web dashboard.
    -   Logs data locally to a CSV file.
    -   Forwards data to ThingSpeak and HiveMQ.

## Hardware Requirements

-   ESP32 Development Board (x2)
-   STM32 "Blue Pill" or similar (x1)
-   ADXL345 Accelerometer Module
-   DS18B20 Temperature Sensor (waterproof version recommended)
-   Analog Water Level Sensor
-   5V Relay Module
-   5V DC Water Pump
-   4.7kΩ Resistor (for DS18B20)
-   Breadboard and Jumper Wires

## Wiring Connections

### 1. Transmitter ESP32 Connections

| Component                | Connection to Transmitter ESP32          | Notes                                     |
| ------------------------ | ---------------------------------------- | ----------------------------------------- |
| **DS18B20 Temp Sensor**  | `VCC` -> `3.3V`, `GND` -> `GND`, `Data` -> `GPIO 4` | Place 4.7kΩ resistor between `VCC` and `Data` |
| **Water Level Sensor**   | `VCC` -> `3.3V`, `GND` -> `GND`, `Analog` -> `GPIO 34` | Check if sensor is 3.3V or 5V compatible |
| **Relay Module**         | `VCC` -> `VIN (5V)`, `GND` -> `GND`, `IN` -> `GPIO 5` | Power relay from a separate 5V source if needed |
| **To Receiver ESP32**    | `TX` (`GPIO 10`) -> Receiver `RX` (`GPIO 9`)   | **Cross-connect TX to RX**            |
|                          | `GND` -> Receiver `GND`                  | **MUST connect Grounds**                  |

### 2. Receiver ESP32 Connections

| Component                | Connection to Receiver ESP32              | Notes                                 |
| ------------------------ | ----------------------------------------- | ------------------------------------- |
| **From STM32**           | STM32 `TX` -> Receiver `RX2` (`GPIO 16`)  | Assumes STM32 is 3.3V logic level     |
|                          | `GND` -> Receiver `GND`                   | **MUST connect Grounds**              |
| **From Transmitter ESP32** | Transmitter `TX` (`GPIO 10`) -> Receiver `RX1` (`GPIO 9`) | Already defined above |

## Software Setup

### 1. Arduino IDE Setup

-   Install the Arduino IDE.
-   Add the ESP32 board manager to your IDE. (Go to `File > Preferences` and add `https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json` to the "Additional Board Manager URLs").
-   Go to `Tools > Board > Boards Manager`, search for "esp32", and install it.
-   Install the "ESP32 Sketch Data Upload" tool. You can find instructions online by searching for `ESP32FS tutorial`.

### 2. Install Libraries

Install the following libraries via the Arduino Library Manager (`Sketch > Include Library > Manage Libraries`):

-   `OneWire` by Paul Stoffregen
-   `DallasTemperature` by Miles Burton
-   `ESPAsyncWebServer` by me-no-dev
-   `AsyncTCP` by me-no-dev
-   `ArduinoJson` by Benoit Blanchon
-   `PubSubClient` by Nick O'Leary

### 3. Flashing the Code

#### For the Transmitter & Receiver ESP32:

1.  Open the corresponding `.ino` file in the Arduino IDE.
2.  Select your ESP32 board from `Tools > Board`.
3.  Select the correct COM port from `Tools > Port`.
4.  Modify the WiFi credentials and API keys in the code if necessary.
5.  **For the Receiver:** Upload the web interface files by selecting `Tools > ESP32 Sketch Data Upload`.
6.  Click the "Upload" button to flash the code to the ESP32.

## How to Use

1.  **Power Up**: Connect power to all three microcontrollers.
2.  **Connect to WiFi**: The Receiver ESP32 will automatically connect to the WiFi network you specified in the code.
3.  **Find the IP Address**: Open the Arduino IDE Serial Monitor for the Receiver ESP32 (baud rate 115200). It will print its IP address once connected to WiFi.
4.  **Access the Dashboard**: Open a web browser on any device connected to the same WiFi network and enter the IP address (e.g., `http://192.168.1.123`).
5.  **Monitor**: You will see the live data dashboard, which updates automatically.

## Remote Access via GitHub Pages (Clarification)

Hosting a *live, real-time server* on GitHub Pages is not possible because it only hosts static files (HTML, CSS, JS). The web server that shows your aquarium's live data is running **directly on your ESP32**.

To access this ESP32 dashboard from anywhere in the world (outside your home network), you need a service that creates a secure tunnel to your device. A popular service for this is **ngrok**. This is an advanced step and is not required for the project to work on your local network.

## Pushing to GitHub

1.  Create a new repository on [GitHub.com](https://github.com).
2.  Open a terminal or command prompt in your `aquarium_automation` project folder.
3.  Initialize a git repository:
    ```bash
    git init
    git add .
    git commit -m "Initial commit of aquarium automation project"
    git branch -M main
    git remote add origin https://github.com/YourUsername/YourRepositoryName.git
    git push -u origin main
    ```

Now your entire project structure, code, and this README file will be on GitHub.
