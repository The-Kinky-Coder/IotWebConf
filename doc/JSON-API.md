# IotWebConf JSON API Documentation

## Overview
The `IotWebConfApi` class provides a RESTful JSON API for interacting with your IotWebConf-enabled device. This feature allows for headless configuration, mobile app integration, or custom frontends (e.g., React/Vue apps) served directly from the device.

**IPv6 on ESP32:** When the ESP32 stack provides IPv6, the API is reachable via both IPv4 and IPv6. Use bracketed IPv6 URLs in clients like curl, e.g. `http://[fd00::1234]/api/status`. The status payload includes `ip_v6` and a boolean `ip_v6_ll` to indicate whether the IPv6 address is link-local.

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

### 5. Automatic Background Scanning
By default, IotWebConfApi automatically scans for WiFi networks every 15 seconds to keep the network cache fresh. This eliminates the need for manual `POST /api/scan` calls in most applications.

#### How It Works
- The API maintains a cached list of available networks
- Every 15 seconds, a background scan is automatically triggered
- Scan results are merged into the cache (existing networks updated, stale networks removed)
- The cache expires networks not seen for 45+ seconds (3 scan intervals)
- Manual scans via `POST /api/scan` are still supported and don't conflict

#### Configuration
You can customize or disable automatic scanning:

**Disable automatic scanning:**
```cpp
void setup() {
  // ... (standard setup)
  api.registerRoutes();
  
  // Disable automatic scanning (manual POST /api/scan required)
  api.setAutoScanEnabled(false);
}
```

**Change scan interval (compile-time):**
```ini
# platformio.ini
build_flags =
  -D IOTWEBCONF_AUTO_SCAN_INTERVAL_MS=30000  ; 30 seconds
```

**Change default enabled state (compile-time):**
```ini
# platformio.ini
build_flags =
  -D IOTWEBCONF_AUTO_SCAN_ENABLED_DEFAULT=false  ; Disabled by default
```

#### API Methods
```cpp
// Disable/enable automatic scanning at runtime
api.setAutoScanEnabled(false);  // Disable
api.setAutoScanEnabled(true);   // Re-enable

// Check current state
bool isEnabled = api.getAutoScanEnabled();
```

#### Benefits
- ✅ **Always up-to-date network list** - No manual intervention required
- ✅ **Smooth user experience** - UI never shows "empty" or stale networks
- ✅ **Efficient** - Async scanning doesn't block main loop
- ✅ **Compatible** - Works with all WebServer types (sync and async)

#### Cache Behavior
The cache system prevents UI flickering:
- `GET /api/networks` always returns results (never empty during scans)
- The `scanning` field indicates if a scan is in progress
- Networks age out gracefully (45-second window)

**Example response during auto-scan:**
```json
{
  "networks": [
    {"ssid": "HomeWiFi", "rssi": -45, "encryption": 3, "lastSeen": 2}
  ],
  "scanning": true
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
  "ip_v4": "192.168.1.105",
  "ip_v6": "fd00::1234",
  "ip_v6_ll": false,
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

---

## Network Management API (Phase 2)

The following endpoints provide advanced network management capabilities, allowing you to:
- List all configured WiFi networks
- Update network credentials programmatically
- Switch between multiple saved networks
- Force reconnection

These endpoints support both **single WiFi mode** (default) and **multiple WiFi mode** (when using `MultipleWifiAddition`).

### 7. Get Configured Networks
Returns a list of all configured WiFi network credentials stored in the device.

*   **URL**: `/api/networks/configured`
*   **Method**: `GET`
*   **Response**: `200 OK`

**Example Response:**
```json
{
  "networks": [
    {
      "id": "iwcWifi0",
      "ssid": "HomeNetwork",
      "isActive": true,
      "hasPassword": true,
      "isCurrent": true
    },
    {
      "id": "wifi2",
      "ssid": "OfficeNetwork",
      "isActive": true,
      "hasPassword": true,
      "isCurrent": false
    },
    {
      "id": "wifi3",
      "ssid": "",
      "isActive": false,
      "hasPassword": false,
      "isCurrent": false
    }
  ]
}
```

**Response Fields:**
- `id`: Network identifier (e.g., "iwcWifi0", "wifi2")
- `ssid`: Network SSID (empty string if not configured)
- `isActive`: Whether the network slot is configured
- `hasPassword`: Whether a password is set
- `isCurrent`: Whether this is the currently connected network

---

### 8. Update Network Credentials
Updates the SSID and password for a specific network slot.

*   **URL**: `/api/networks/{id}`
*   **Method**: `POST`
*   **Content-Type**: `application/json`
*   **Path Parameter**: `{id}` - Network identifier (e.g., "iwcWifi0", "wifi2")

**Request Body:**
```json
{
  "ssid": "NewNetworkName",
  "password": "newpassword123"
}
```

**Response**: `200 OK`
```json
{
  "status": "updated",
  "id": "wifi2"
}
```

**Error Responses:**
- `400 Bad Request`: Missing SSID or invalid JSON
  ```json
  { "error": "SSID field required" }
  ```
- `404 Not Found`: Network ID not found
  ```json
  { "error": "Network not found" }
  ```

**⚠️ Important Notes:**
- This endpoint **writes to EEPROM immediately**
- EEPROM has limited write cycles (~100,000 on ESP8266/ESP32)
- Avoid calling this endpoint repeatedly in a loop
- The password field is optional; omit it for open networks

**Example:**
```bash
curl -X POST http://192.168.1.100/api/networks/wifi2 \
  -H "Content-Type: application/json" \
  -d '{"ssid":"MyNetwork","password":"MyPassword"}'

# IPv6 (ESP32 dual-stack)
curl -X POST "http://[fd00::1234]/api/networks/wifi2" \
  -H "Content-Type: application/json" \
  -d '{"ssid":"MyNetwork","password":"MyPassword"}'
```

---

### 9. Connect to Network
Connects to a specific saved network by copying its credentials to the primary WiFi slot and triggering a connection attempt.

*   **URL**: `/api/networks/{id}/connect`
*   **Method**: `POST`
*   **Path Parameter**: `{id}` - Network identifier (e.g., "iwcWifi0", "wifi2")

**Response**: `200 OK`
```json
{
  "status": "connecting",
  "id": "wifi2",
  "ssid": "OfficeNetwork"
}
```

**Error Responses:**
- `400 Bad Request`: Network not configured (SSID empty)
  ```json
  { "error": "Network not configured (SSID empty)" }
  ```
- `404 Not Found`: Network ID not found
  ```json
  { "error": "Network not found" }
  ```

**How It Works:**
1. Retrieves credentials from the specified network slot
2. Copies them to the primary WiFi parameter group (`iwcWifi0`)
3. Saves configuration to EEPROM
4. Calls `forceWifiConnection()` to trigger immediate connection

**Example:**
```bash
curl -X POST http://192.168.1.100/api/networks/wifi2/connect

# IPv6 (ESP32 dual-stack)
curl -X POST "http://[fd00::1234]/api/networks/wifi2/connect"
```

---

### 10. Reconnect WiFi
Forces a WiFi reconnection attempt using the current configuration. Useful after network changes or when experiencing connection issues.

*   **URL**: `/api/reconnect`
*   **Method**: `POST`

**Response**: `200 OK`
```json
{
  "status": "reconnecting"
}
```

**Key Differences from Connect:**
- Does **not** modify configuration
- Does **not** write to EEPROM
- Uses current WiFi credentials
- Useful for recovering from temporary network issues

**Example:**
```bash
curl -X POST http://192.168.1.100/api/reconnect

# IPv6 (ESP32 dual-stack)
curl -X POST "http://[fd00::1234]/api/reconnect"
```

---

## Multiple WiFi Support

To use the network management API with multiple WiFi networks, you must:

### 1. Enable MultipleWifiAddition

```cpp
#include <IotWebConfMultipleWifi.h>

// Define additional WiFi parameter groups
char wifi2Ssid[IOTWEBCONF_WORD_LEN];
char wifi2Pass[IOTWEBCONF_PASSWORD_LEN];
ChainedWifiParameterGroup wifi2Group("Office WiFi", "wifi2", wifi2Ssid, wifi2Pass);

char wifi3Ssid[IOTWEBCONF_WORD_LEN];
char wifi3Pass[IOTWEBCONF_PASSWORD_LEN];
ChainedWifiParameterGroup wifi3Group("Guest WiFi", "wifi3", wifi3Ssid, wifi3Pass);

// Create array of WiFi groups
ChainedWifiParameterGroup* wifiChain[] = { &wifi2Group, &wifi3Group };

// Create MultipleWifiAddition
MultipleWifiAddition multiWifi(
  wifiChain,
  sizeof(wifiChain) / sizeof(wifiChain[0]),
  []() { iotWebConf.saveConfig(); }
);
```

### 2. Register with IotWebConf

```cpp
void setup() {
  // Add to IotWebConf parameter groups
  iotWebConf.addParameterGroup(&multiWifi);
  
  // Tell the API about MultipleWifiAddition
  api.setMultipleWifiAddition(&multiWifi);
  
  iotWebConf.init();
  api.registerRoutes();
}
```

### 3. Network ID Reference

| Network ID | Description | Always Present? |
|------------|-------------|-----------------|
| `iwcWifi0` | Primary WiFi network | ✅ Yes |
| `wifi2`, `wifi3`, ... | Additional networks | Only if MultipleWifiAddition is configured |
| Custom IDs | User-defined ChainedWifiParameterGroup | Only if added by user |

The network ID is the second parameter when creating a `ChainedWifiParameterGroup`:
```cpp
ChainedWifiParameterGroup wifiGroup("Display Label", "networkId", ssid, password);
                                                      // ↑ This is the ID
```

---

## AsyncWebServer Support

The network management API also supports **ESPAsyncWebServer**. To use it:

### 1. Include AsyncWebServer Headers

```cpp
#include <ESPAsyncWebServer.h>
#include <IotWebConf.h>
#include <IotWebConfApi.h>
```

### 2. Create API with AsyncWebServer

```cpp
AsyncWebServer server(80);
IotWebConf iotWebConf(thingName, &dnsServer, &server, wifiInitialApPassword);

// Create API instance with AsyncWebServer
IotWebConfApi api(&iotWebConf, &server);
```

### 3. Register Async Routes

```cpp
void setup() {
  iotWebConf.init();
  
  // Use registerRoutesAsync() for AsyncWebServer
  api.registerRoutesAsync();
}
```

**Key Differences:**
- Use `registerRoutesAsync()` instead of `registerRoutes()`
- No need to call `server.handleClient()` (AsyncWebServer is event-driven)
- Path parameters (`{id}`) are handled with regex patterns

---

## Complete Usage Example

Here's a complete example using all network management features:

```cpp
#include <IotWebConf.h>
#include <IotWebConfApi.h>
#include <IotWebConfMultipleWifi.h>

const char thingName[] = "NetworkManager";
const char wifiInitialApPassword[] = "password123";
#define CONFIG_VERSION "nm1"

DNSServer dnsServer;
WebServer server(80);
IotWebConf iotWebConf(thingName, &dnsServer, &server, wifiInitialApPassword, CONFIG_VERSION);
IotWebConfApi api(&iotWebConf, &server);

// Multiple WiFi setup
char wifi2Ssid[IOTWEBCONF_WORD_LEN];
char wifi2Pass[IOTWEBCONF_PASSWORD_LEN];
ChainedWifiParameterGroup wifi2Group("Office WiFi", "wifi2", wifi2Ssid, wifi2Pass);

ChainedWifiParameterGroup* wifiChain[] = { &wifi2Group };
MultipleWifiAddition multiWifi(wifiChain, 1, []() { iotWebConf.saveConfig(); });

void setup() {
  Serial.begin(115200);
  
  iotWebConf.setStatusPin(LED_BUILTIN);
  iotWebConf.setConfigPin(0);
  iotWebConf.addParameterGroup(&multiWifi);
  iotWebConf.init();
  
  // Register routes
  server.on("/", []() {
    server.send(200, "text/html", "<h1>Network Manager</h1>");
  });
  server.on("/config", []{ iotWebConf.handleConfig(); });
  server.onNotFound([]{ iotWebConf.handleNotFound(); });
  
  // Enable network management API
  api.setMultipleWifiAddition(&multiWifi);
  api.registerRoutes();
  
  Serial.println("Ready!");
}

void loop() {
  iotWebConf.doLoop();
  api.loop();
}
```

---

## API Workflow Examples

### Switching Between Networks

```javascript
// 1. Get list of configured networks
const networks = await fetch('/api/networks/configured').then(r => r.json());

// 2. Find the network you want
const officeNet = networks.networks.find(n => n.id === 'wifi2');

// 3. Connect to it
await fetch(`/api/networks/${officeNet.id}/connect`, { method: 'POST' });

// 4. Wait a few seconds, then check status
setTimeout(async () => {
  const status = await fetch('/api/status').then(r => r.json());
  console.log('Connected to:', status.wifi.ssid);
}, 5000);
```

### Adding a New Network (to existing slot)

```javascript
// Update credentials for network slot 'wifi3'
await fetch('/api/networks/wifi3', {
  method: 'POST',
  headers: { 'Content-Type': 'application/json' },
  body: JSON.stringify({
    ssid: 'GuestNetwork',
    password: 'guest123'
  })
});

// Connect to the newly configured network
await fetch('/api/networks/wifi3/connect', { method: 'POST' });
```

### Recovering from Connection Issues

```javascript
// Force reconnection without modifying config
await fetch('/api/reconnect', { method: 'POST' });

// Check connection status after a few seconds
setTimeout(async () => {
  const status = await fetch('/api/status').then(r => r.json());
  if (status.wifi.status === 3) { // WL_CONNECTED
    console.log('Successfully reconnected');
  } else {
    console.log('Still disconnected');
  }
}, 5000);
```

---

## Error Handling

All endpoints return appropriate HTTP status codes and error messages:

### Common Error Responses

**400 Bad Request** - Invalid input
```json
{
  "error": "SSID field required"
}
```

**404 Not Found** - Resource not found
```json
{
  "error": "Network not found"
}
```

**500 Internal Server Error** - Server error
```json
{
  "error": "Failed to save configuration"
}
```

### Error Handling Best Practices

```javascript
async function updateNetwork(id, ssid, password) {
  try {
    const response = await fetch(`/api/networks/${id}`, {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ ssid, password })
    });
    
    if (!response.ok) {
      const error = await response.json();
      throw new Error(error.error || 'Request failed');
    }
    
    return await response.json();
  } catch (error) {
    console.error('Failed to update network:', error);
    throw error;
  }
}
```

---

## Security Considerations

⚠️ **The examples provided do not include authentication.** In production environments:

### 1. Enable HTTP Basic Authentication

```cpp
server.on("/api/networks/configured", [&]() {
  if (!server.authenticate("admin", "securepassword")) {
    return server.requestAuthentication();
  }
  api.handleApiGetConfiguredNetworks();
});
```

### 2. Use HTTPS (TLS)

Consider using HTTPS to encrypt communication:
- ESP8266: Use `ESP8266WebServerSecure`
- ESP32: Use secure WiFi libraries

### 3. Implement Rate Limiting

Prevent abuse by limiting API requests:

```cpp
unsigned long lastApiCall = 0;
const unsigned long API_RATE_LIMIT = 1000; // 1 second

if (millis() - lastApiCall < API_RATE_LIMIT) {
  server.send(429, "application/json", "{\"error\":\"Rate limit exceeded\"}");
  return;
}
lastApiCall = millis();
```

### 4. Validate Input

Always validate and sanitize input data:

```cpp
// Check SSID length
if (strlen(ssid) > IOTWEBCONF_WORD_LEN) {
  server.send(400, "application/json", "{\"error\":\"SSID too long\"}");
  return;
}

// Check for invalid characters
if (strchr(ssid, '\0') || strchr(ssid, '\n')) {
  server.send(400, "application/json", "{\"error\":\"Invalid SSID\"}");
  return;
}
```

---

## Performance Notes

### EEPROM Wear

⚠️ **Important**: EEPROM has limited write cycles (~100,000 on ESP8266/ESP32)

**Endpoints that write to EEPROM:**
- `POST /api/networks/{id}` - Updates credentials
- `POST /api/networks/{id}/connect` - Copies and saves credentials
- `POST /api/config` - Updates configuration

**Endpoints that do NOT write to EEPROM:**
- `POST /api/reconnect` - Only triggers reconnection
- `GET` endpoints - Read-only

**Best Practices:**
- Batch credential updates when possible
- Use `POST /api/reconnect` for temporary reconnections
- Avoid calling write endpoints in loops or timers

### Memory Usage

The JSON API uses **ArduinoJson v6** with dynamic memory allocation:
- Typical memory usage: ~2-4KB per request
- Large configurations may require adjusting `JsonDocument` sizes
- Monitor heap usage with `ESP.getFreeHeap()`

### Async Operations

WiFi scanning is asynchronous to prevent blocking:

```cpp
// 1. Start scan (non-blocking)
await fetch('/api/scan', { method: 'POST' });

// 2. Poll for results
while (true) {
  await new Promise(resolve => setTimeout(resolve, 1000));
  const response = await fetch('/api/networks');
  if (response.status === 200) {
    const data = await response.json();
    console.log('Scan complete:', data.networks);
    break;
  }
  // 202 = still scanning
}
```

---

## Troubleshooting

### Network Not Found

**Problem:** `{"error": "Network not found"}`

**Solutions:**
1. Check network ID is correct (case-sensitive)
2. Verify `MultipleWifiAddition` is set: `api.setMultipleWifiAddition(&multiWifi)`
3. Confirm network was added to `wifiChain[]` array
4. Check `ChainedWifiParameterGroup` ID matches request

### CORS Issues

**Problem:** JavaScript fetch fails due to CORS

**Solution:** Add CORS headers (already included in API):
```cpp
server.sendHeader("Access-Control-Allow-Origin", "*");
```

For production, restrict origins:
```cpp
server.sendHeader("Access-Control-Allow-Origin", "https://myapp.com");
```

### Connection Hangs

**Problem:** Connection attempt never completes

**Possible Causes:**
1. Network out of range
2. Incorrect credentials
3. Device in AP mode (no STA connection)
4. WiFi module busy

**Solutions:**
- Check `GET /api/status` for current WiFi status
- Use `POST /api/reconnect` to retry
- Reboot device if stuck: `ESP.restart()`

### Memory Issues

**Problem:** Device crashes or resets during API calls

**Solutions:**
1. Increase heap size (reduce other allocations)
2. Use smaller JSON documents
3. Avoid storing large strings in PROGMEM
4. Monitor memory: `Serial.println(ESP.getFreeHeap())`

---

## Examples

Complete working examples are available in the `examples/` directory:

- **IotWebConf18JsonApi** - Basic JSON API usage with single WiFi
- **IotWebConf20NetworkManagement** - Network management API with multiple WiFi support and interactive web UI
- **IotWebConf19AsyncServer** - AsyncWebServer integration example

---

## API Endpoint Summary

| Endpoint | Method | Description | EEPROM Write |
|----------|--------|-------------|--------------|
| `/api/status` | GET | Device status and WiFi info | No |
| `/api/scan` | POST | Start WiFi network scan | No |
| `/api/networks` | GET | Get scan results | No |
| `/api/wifi` | POST | Connect to WiFi | Yes |
| `/api/config` | GET | Get configuration | No |
| `/api/config` | POST | Update configuration | Yes |
| `/api/networks/configured` | GET | List all configured networks | No |
| `/api/networks/{id}` | POST | Update network credentials | Yes |
| `/api/networks/{id}/connect` | POST | Connect to specific network | Yes |
| `/api/reconnect` | POST | Force reconnection | No |

---

## Version History

### Phase 3 (Unreleased) - Automatic Background Scanning
- Added automatic WiFi scanning every 15 seconds (default)
- Added `setAutoScanEnabled()` / `getAutoScanEnabled()` methods
- Added compile-time constants: `IOTWEBCONF_AUTO_SCAN_INTERVAL_MS` and `IOTWEBCONF_AUTO_SCAN_ENABLED_DEFAULT`
- Improved network cache system with 45-second stale timeout
- Manual `POST /api/scan` still supported for on-demand scanning

### Phase 2 (v3.2.0) - Network Management API
- Added `GET /api/networks/configured`
- Added `POST /api/networks/{id}`
- Added `POST /api/networks/{id}/connect`
- Added `POST /api/reconnect`
- Added AsyncWebServer support via `registerRoutesAsync()`
- Added `setMultipleWifiAddition()` for multi-network management

### IPv6 Notes (ESP32 dual-stack)
- API endpoints are reachable via IPv6 when the ESP32 network stack assigns an IPv6 address
- Use bracketed IPv6 URLs in clients (e.g., `http://[fd00::1234]/api/status`)

### Phase 1 (v3.1.0) - Core JSON API
- Initial JSON API implementation
- Basic endpoints: status, scan, networks, wifi, config

---

## Additional Resources

- **GitHub Repository**: [https://github.com/prampec/IotWebConf](https://github.com/prampec/IotWebConf)
- **ArduinoJson Documentation**: [https://arduinojson.org/](https://arduinojson.org/)
- **API Design Documentation**: See `AgentProgress/009-API-Architecture-Design.md`
- **Implementation Notes**: See `AgentProgress/012-Phase2A-Implementation-Complete.md`

---

## License

This library and documentation are released under the MIT License.  
Copyright (C) 2020 Balazs Kelemen <prampec+arduino@gmail.com>
