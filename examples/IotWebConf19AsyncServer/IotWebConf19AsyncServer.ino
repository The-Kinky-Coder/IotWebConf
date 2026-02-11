/**
 * IotWebConf19AsyncServer.ino -- IotWebConf is an ESP8266/ESP32
 *   non blocking WiFi/AP web configuration library for Arduino.
 *   https://github.com/prampec/IotWebConf 
 *
 * Copyright (C) 2020 Balazs Kelemen <prampec+arduino@gmail.com>
 *
 * This software may be modified and distributed under the terms
 * of the MIT license.  See the LICENSE file for details.
 */

/**
 * Example: AsyncWebServer Integration
 * Description:
 *   This example demonstrates how to use IotWebConf with ESPAsyncWebServer.
 *   Instead of the synchronous WebServer, this allows your application to share
 *   an AsyncWebServer instance with IotWebConf.
 *   
 *   This is particularly useful when:
 *   - Your main application already uses AsyncWebServer
 *   - You want to serve other web content alongside IotWebConf
 *   - You need the performance benefits of async handling
 *   
 *   The example shows:
 *   - How to instantiate IotWebConf with AsyncWebServer
 *   - How to add your own async routes
 *   - How the captive portal and config page work with async
 *
 *   After starting up the thing, search for WiFi access points (e.g. with your phone).
 *   Connect using the password provided in the code.
 *   The captive portal will automatically appear.
 *   
 *   Set a new password for the Thing as well as your local WiFi credentials.
 *   After configuration, visit the root page to see both IotWebConf and your custom
 *   async routes working together.
 *
 *   ESP32 IPv6 note: if the board has IPv6, use bracketed URLs like
 *   http://[fd00::1234]/ or http://[fd00::1234]/api/status
 */

#include <IotWebConf.h>
#include <ESPAsyncWebServer.h>

// -- Initial name of the Thing. Used e.g. as SSID of the own Access Point.
const char thingName[] = "asyncThing";

// -- Initial password to connect to the Thing, when it creates an own Access Point.
const char wifiInitialApPassword[] = "async8266";

// -- Configuration version
#define CONFIG_VERSION "async1"

// -- Status indicator pin (LED)
// Note: LED_BUILTIN may not be defined on all boards (e.g., ESP32-S3-USB-OTG)
// If compilation fails, replace with a valid GPIO number or -1 to disable
#ifndef LED_BUILTIN
#define LED_BUILTIN 2  // Default to GPIO 2 if LED_BUILTIN not defined
#endif
#define STATUS_PIN LED_BUILTIN

// -- Method declarations
String getHtmlPage();

// -- Server and DNS instances
DNSServer dnsServer;
AsyncWebServer server(80);

// -- IotWebConf instance with AsyncWebServer support
IotWebConf iotWebConf(thingName, &dnsServer, &server, wifiInitialApPassword, CONFIG_VERSION);

// -- Counter to demonstrate dynamic content
unsigned long requestCounter = 0;

void setup() 
{
  Serial.begin(115200);
  Serial.println();
  Serial.println("Starting AsyncWebServer example...");

  // -- Configure status LED
  iotWebConf.setStatusPin(STATUS_PIN);

  // -- Initialize IotWebConf
  iotWebConf.init();

  // -- OPTION 1: Automatic registration (simplified approach)
  // Register IotWebConf's core routes automatically (/, /config, 404)
  // This must be called BEFORE server.begin() and AFTER any custom routes
  // that you want to take precedence over IotWebConf's default handling
  
  // Custom API endpoint (add BEFORE setupRequestHandlers to take precedence)
  server.on("/api/status", HTTP_GET, [](AsyncWebServerRequest *request){
    String json = "{";
    json += "\"device\":\"" + String(thingName) + "\",";
    json += "\"requests\":" + String(requestCounter) + ",";
    json += "\"uptime\":" + String(millis()) + ",";
    json += "\"heap\":" + String(ESP.getFreeHeap());
    json += "}";
    request->send(200, "application/json", json);
  });

  // Custom root page handler (overrides IotWebConf's default)
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    // Let IotWebConf handle captive portal requests
    AsyncWebRequestWrapper wrapper(request);
    if (iotWebConf.handleCaptivePortal(&wrapper))
    {
      // Captive portal was served
      return;
    }
    
    requestCounter++;
    String html = getHtmlPage();
    request->send(200, "text/html", html);
  });

  // Now register IotWebConf's core handlers
  // This will add /config (GET and POST) and 404 handler
  iotWebConf.setupRequestHandlers();

  /* OPTION 2: Manual registration (for more control)
   * If you need more control or want to customize the handling,
   * you can register routes manually:
   * 
   * server.on("/config", HTTP_GET, [](AsyncWebServerRequest *request){
   *   AsyncWebRequestWrapper wrapper(request);
   *   iotWebConf.handleConfig(&wrapper);
   * });
   * 
   * server.on("/config", HTTP_POST, [](AsyncWebServerRequest *request){
   *   AsyncWebRequestWrapper wrapper(request);
   *   iotWebConf.handleConfig(&wrapper);
   * });
   * 
   * server.onNotFound([](AsyncWebServerRequest *request){
   *   AsyncWebRequestWrapper wrapper(request);
   *   iotWebConf.handleNotFound(&wrapper);
   * });
   */

  Serial.println("Ready.");
  Serial.print("Initial AP Password: ");
  Serial.println(wifiInitialApPassword);
}

void loop() 
{
  // -- doLoop should be called as frequently as possible
  iotWebConf.doLoop();
  
  // Your application code goes here
  // (No need to call server.handleClient() - async handles it automatically!)
}

/**
 * Generate the HTML page with embedded styling
 */
String getHtmlPage()
{
  String html = "<!DOCTYPE html><html lang=\"en\">";
  html += "<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\"/>";
  html += "<title>IotWebConf AsyncWebServer Example</title>";
  html += "<style>";
  html += "body { font-family: Arial, sans-serif; margin: 40px; background: #f0f0f0; }";
  html += ".container { background: white; padding: 20px; border-radius: 8px; max-width: 600px; margin: 0 auto; box-shadow: 0 2px 4px rgba(0,0,0,0.1); }";
  html += "h1 { color: #333; }";
  html += ".info { background: #e3f2fd; padding: 15px; border-radius: 4px; margin: 10px 0; }";
  html += ".button { display: inline-block; background: #2196F3; color: white; padding: 10px 20px; text-decoration: none; border-radius: 4px; margin: 5px; }";
  html += ".button:hover { background: #1976D2; }";
  html += "</style></head><body>";
  
  html += "<div class=\"container\">";
  html += "<h1>🚀 AsyncWebServer + IotWebConf</h1>";
  
  html += "<div class=\"info\">";
  html += "<strong>Device:</strong> " + String(thingName) + "<br>";
  html += "<strong>Request Count:</strong> " + String(requestCounter) + "<br>";
  html += "<strong>Uptime:</strong> " + String(millis() / 1000) + " seconds<br>";
  html += "<strong>Free Heap:</strong> " + String(ESP.getFreeHeap()) + " bytes";
  html += "</div>";
  
  html += "<h2>Features Demonstrated</h2>";
  html += "<ul>";
  html += "<li>✅ IotWebConf with ESPAsyncWebServer</li>";
  html += "<li>✅ Shared server instance</li>";
  html += "<li>✅ Custom async routes</li>";
  html += "<li>✅ Captive portal support</li>";
  html += "<li>✅ Dynamic content</li>";
  html += "</ul>";
  
  html += "<h2>Available Endpoints</h2>";
  html += "<a href=\"/\" class=\"button\">Home</a>";
  html += "<a href=\"/config\" class=\"button\">⚙️ Configuration</a>";
  html += "<a href=\"/api/status\" class=\"button\">📊 API Status</a>";
  
  html += "<h2>How It Works</h2>";
  html += "<p>This page is served by an AsyncWebServer handler you defined in your code. ";
  html += "The <code>/config</code> endpoint is handled by IotWebConf. ";
  html += "Both use the same AsyncWebServer instance!</p>";
  
  html += "<p><small>Refresh this page to see the request counter increase.</small></p>";
  
  html += "</div></body></html>";
  
  return html;
}
