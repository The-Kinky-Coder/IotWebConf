# IotWebConf JSON API Documentation

## Overview
The `IotWebConfApi` class provides a RESTful JSON API for interacting with your IotWebConf-enabled device. This feature allows for headless configuration, mobile app integration, or custom frontends (e.g., React/Vue apps) served directly from the device.

## Prerequisites
1.  **ArduinoJson**: This library depends on [ArduinoJson](https://arduinojson.org/) (v6).
2.  **Build Flag**: You must define `IOTWEBCONF_ENABLE_JSON` in your build configuration (e.g., `platformio.ini`).

## Setup

### 1. Include Headers
```cpp
#include <IotWebConf.h>
#include <IotWebConfApi.h>
```

### 2. Instantiate API Class
Create the API instance by passing your `IotWebConf` and `WebServer` objects.

```cpp
WebServer server(80);
IotWebConf iotWebConf(thingName, &dnsServer, &server, wifiInitialApPassword, CONFIG_VERSION);

// Create the API instance
IotWebConfApi api(&iotWebConf, &server);
```

### 3. Register Routes
In your `setup()` function, call `registerRoutes()` to bind the API endpoints to the web server.

```cpp
void setup() {
  // ... (standard setup)
  iotWebConf.init();
  
  // Standard IotWebConf handlers
  server.on("/", handleRoot);
  server.on("/config", []{ iotWebConf.handleConfig(); });
  server.onNotFound([](){ iotWebConf.handleNotFound(); });
  
  // Register JSON API handlers
  api.registerRoutes();
  
  // ...
}
```

### 4. Loop
Call the `loop()` method in your main `loop()` function to handle asynchronous tasks (like WiFi scanning).

```cpp
void loop() {
  iotWebConf.doLoop();
  api.loop(); 
}
```

## API Reference

### 1. Get Status
Returns the current device state, WiFi connection status, and Access Point details.

*   **URL**: `/api/status`
*   **Method**: `GET`
*   **Response**: `200 OK`

**Example Response:**
```json
{
  "state": 4,
  "thingName": "testThing",
  "ip": "192.168.1.105",
  "mac": "AA:BB:CC:DD:EE:FF",
  "wifi": {
    "status": 3,
    "ssid": "MyHomeWifi",
    "rssi": -65
  },
  "ap": {
    "active": 0,
    "clients": 0,
    "ip": "192.168.4.1"
  }
}
```
*Note: `state` corresponds to the `NetworkState` enum (Boot=0, NotConfigured=1, ApMode=2, Connecting=3, OnLine=4, OffLine=5).*

---

### 2. Scan Networks
Triggers a background WiFi scan. This operation is non-blocking. The results are retrieved via `/api/networks`.

*   **URL**: `/api/scan`
*   **Method**: `POST`
*   **Response**: `202 Accepted`

**Example Response:**
```json
{
  "status": "Scan started"
}
```

---

### 3. Get Networks
Retrieves the results of the last WiFi scan.

*   **URL**: `/api/networks`
*   **Method**: `GET`
*   **Response Codes**:
    *   `200 OK`: Scan results available.
    *   `202 Accepted`: Scan is still in progress.
    *   `400 Bad Request`: No scan has been triggered.

**Example Response (Success):**
```json
{
  "networks": [
    {
      "ssid": "HomeNetwork",
      "rssi": -50,
      "encryption": 7
    },
    {
      "ssid": "GuestWiFi",
      "rssi": -80,
      "encryption": 4
    }
  ]
}
```

---

### 4. Connect to WiFi
Configures the device to connect to a specific WiFi network. This updates the internal WiFi settings, saves them to EEPROM, and immediately triggers a connection attempt.

*   **URL**: `/api/wifi`
*   **Method**: `POST`
*   **Content-Type**: `application/json`

**Request Body:**
```json
{
  "ssid": "MyNetwork",
  "password": "MyPassword"
}
```

**Response**: `200 OK`
```json
{ "status": "Connecting" }
```

---

### 5. Get Configuration
Returns all current configuration parameters (System and Custom groups) as a JSON object.

*   **URL**: `/api/config`
*   **Method**: `GET`
*   **Response**: `200 OK`

**Example Response:**
```json
{
  "iwcThingName": "testThing",
  "iwcApPassword": "",
  "iwcWifi0": {
    "iwcWifiSsid": "MyNetwork",
    "iwcWifiPassword": "MyPassword"
  },
  "stringParam": "CurrentValue",
  "intParam": "20"
}
```

---

### 6. Set Configuration
Updates configuration parameters. Keys must match the parameter IDs defined in your code (e.g., `iwcThingName`, `stringParam`).

*   **URL**: `/api/config`
*   **Method**: `POST`
*   **Content-Type**: `application/json`

**Request Body:**
```json
{
  "stringParam": "NewValue",
  "intParam": "50"
}
```

**Response**: `200 OK`
```json
{ "status": "Saved" }
```
