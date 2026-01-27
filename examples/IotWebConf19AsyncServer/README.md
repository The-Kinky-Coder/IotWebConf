# IotWebConf Example 19: AsyncWebServer Integration

## Overview

This example demonstrates how to use IotWebConf with **ESPAsyncWebServer** instead of the standard synchronous WebServer. This allows your application to share a single AsyncWebServer instance with IotWebConf, enabling you to serve your own content while still benefiting from IotWebConf's configuration management and captive portal.

## Features Demonstrated

- ✅ **Shared AsyncWebServer**: One server instance for both IotWebConf and your app
- ✅ **Custom Async Routes**: Add your own request handlers
- ✅ **Captive Portal**: Works seamlessly with AsyncWebServer
- ✅ **Configuration Page**: IotWebConf's config portal works as expected
- ✅ **API Endpoint**: Example REST API alongside IotWebConf
- ✅ **Dynamic Content**: Request counter and system stats

## Hardware Requirements

- ESP8266 or ESP32 board
- Built-in LED (for status indication)

## Software Requirements

- Arduino IDE or PlatformIO
- ESPAsyncWebServer library
- ESPAsyncTCP library (ESP8266) or AsyncTCP (ESP32)
- IotWebConf library (this library)

## Installation

### Arduino IDE

1. Install ESPAsyncWebServer:
   - ESP8266: https://github.com/me-no-dev/ESPAsyncWebServer
   - ESP32: https://github.com/me-no-dev/ESPAsyncWebServer

2. Install dependencies:
   - ESP8266: ESPAsyncTCP (https://github.com/me-no-dev/ESPAsyncTCP)
   - ESP32: AsyncTCP (https://github.com/me-no-dev/AsyncTCP)

3. Open `IotWebConf19AsyncServer.ino` in Arduino IDE

4. Select your board and port

5. Upload!

### PlatformIO

```ini
[env:esp32dev]
platform = espressif32
board = esp32dev
framework = arduino
lib_deps = 
    prampec/IotWebConf@^3.2.1
    https://github.com/me-no-dev/ESPAsyncWebServer.git
    https://github.com/me-no-dev/AsyncTCP.git
```

For ESP8266:
```ini
[env:esp12e]
platform = espressif8266
board = esp12e
framework = arduino
lib_deps = 
    prampec/IotWebConf@^3.2.1
    https://github.com/me-no-dev/ESPAsyncWebServer.git
    https://github.com/me-no-dev/ESPAsyncTCP.git
```

## Usage

### 1. First Boot (AP Mode)

1. **Upload the sketch** to your device
2. **Look for WiFi network** named `asyncThing`
3. **Connect** using password: `async8266`
4. **Browser opens automatically** (captive portal)
5. **Configure settings**:
   - Set a new AP password
   - Enter your WiFi SSID and password
6. **Click Apply**
7. **Disconnect from AP** to let device connect to your WiFi

### 2. Normal Operation

Once configured and connected to your WiFi:

1. **Find device IP** (check router or serial monitor)
2. **Visit** `http://device-ip/`
3. **Explore endpoints**:
   - `/` - Home page with statistics
   - `/config` - Configuration page (requires password)
   - `/api/status` - JSON status endpoint

### 3. Reconfiguration

To change settings later:

1. Visit `http://device-ip/config`
2. Log in with username `admin` and your AP password
3. Update settings
4. Click Apply

## Code Highlights

### Creating IotWebConf with AsyncWebServer

```cpp
#include <IotWebConf.h>
#include <ESPAsyncWebServer.h>

DNSServer dnsServer;
AsyncWebServer server(80);
IotWebConf iotWebConf(thingName, &dnsServer, &server, wifiInitialApPassword);
```

That's it! Just pass your AsyncWebServer instance to IotWebConf.

### Adding Custom Routes

```cpp
void setup() {
  iotWebConf.init();
  
  // Your custom async route
  server.on("/api/status", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(200, "application/json", "{\"status\":\"OK\"}");
  });
  
  // IotWebConf routes
  server.on("/config", HTTP_GET, [](AsyncWebServerRequest *request){
    AsyncWebRequestWrapper wrapper(request);
    iotWebConf.handleConfig(&wrapper);
  });
}
```

### Handling Captive Portal

```cpp
server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
  // Let IotWebConf handle captive portal
  if (iotWebConf.handleCaptivePortal()) {
    return;
  }
  
  // Your normal page handling
  request->send(200, "text/html", myPage);
});
```

## Key Differences from Synchronous WebServer

### AsyncWebServer (This Example)
```cpp
// No handleClient() needed in loop!
void loop() {
  iotWebConf.doLoop();
  // That's it!
}
```

### Synchronous WebServer
```cpp
// Must call handleClient()
void loop() {
  iotWebConf.doLoop();
  server.handleClient();  // Required!
}
```

## Troubleshooting

### Issue: Code doesn't compile

**Solution**: Make sure ESPAsyncWebServer and its dependencies are installed correctly.

### Issue: Captive portal doesn't work

**Solution**: Make sure you've initialized IotWebConf before adding your routes:
```cpp
iotWebConf.init();  // Do this first
server.on(...);     // Then add routes
```

### Issue: Config page shows 404

**Solution**: Make sure you're wrapping the request in AsyncWebRequestWrapper:
```cpp
server.on("/config", HTTP_POST, [](AsyncWebServerRequest *request){
  AsyncWebRequestWrapper wrapper(request);  // Wrap it!
  iotWebConf.handleConfig(&wrapper);
});
```

## Learn More

- **IotWebConf Documentation**: [README.md](../../README.md)
- **Async Integration Guide**: [doc/HeadlessIntegration.md](../../doc/HeadlessIntegration.md)
- **ESPAsyncWebServer**: https://github.com/me-no-dev/ESPAsyncWebServer

## License

This example is part of IotWebConf and is released under the MIT License.
