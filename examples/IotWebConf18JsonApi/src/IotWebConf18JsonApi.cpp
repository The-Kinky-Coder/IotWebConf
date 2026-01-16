#include <Arduino.h>
#include <IotWebConf.h>
#include <IotWebConfUsing.h> // This loads aliases for easier class names.
#include <IotWebConfApi.h>

const char thingName[] = "testThing";
const char wifiInitialApPassword[] = "smrtTHNG8266";

#define CONFIG_VERSION "dem3"
#define STATUS_PIN LED_BUILTIN
#define CONFIG_PIN D2

DNSServer dnsServer;
WebServer server(80);
IotWebConf iotWebConf(thingName, &dnsServer, &server, wifiInitialApPassword, CONFIG_VERSION);

// -- API instance
IotWebConfApi api(&iotWebConf, &server);

// -- Parameters
char stringParamValue[128];
IotWebConfTextParameter stringParam = IotWebConfTextParameter("String param", "stringParam", stringParamValue, 128);

void handleRoot()
{
  if (iotWebConf.handleCaptivePortal()) { return; }
  String s = "<!DOCTYPE html><html lang=\"en\"><head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1, user-scalable=no\"/>";
  s += "<title>IotWebConf 18 Json Api</title></head><body>Hello world!";
  s += "<ul>";
  s += "<li>String param value: ";
  s += stringParamValue;
  s += "</ul>";
  s += "Go to <a href='config'>configure page</a> to change values.";
  s += "</body></html>\n";
  server.send(200, "text/html", s);
}

void setup() 
{
  Serial.begin(115200);
  Serial.println();
  Serial.println("Starting up...");

  iotWebConf.setStatusPin(STATUS_PIN);
  iotWebConf.setConfigPin(CONFIG_PIN);
  iotWebConf.addSystemParameter(&stringParam);

  iotWebConf.init();

  // -- Set up required URL handlers on the web server.
  server.on("/", handleRoot);
  server.on("/config", []{ iotWebConf.handleConfig(); });
  server.onNotFound([](){ iotWebConf.handleNotFound(); });
  
  // -- Register API routes
  api.registerRoutes();

  Serial.println("Ready.");
}

void loop() 
{
  iotWebConf.doLoop();
  api.loop(); // Handle async API tasks
}
