/**
 * IotWebConfApi.h -- JSON API for IotWebConf
 * 
 * This class adds JSON API endpoints to an existing IotWebConf instance.
 * 
 * Endpoints:
 * - GET /api/status
 * - GET /api/networks
 * - POST /api/wifi
 * - GET /api/config
 * - POST /api/config
 * - POST /api/scan
 */

#ifndef IotWebConfApi_h
#define IotWebConfApi_h

#include <ArduinoJson.h>
#ifdef ESP8266
# include <ESP8266WiFi.h>
# include <ESP8266WebServer.h>
#elif defined(ESP32)
# include <WiFi.h>
# include <WebServer.h>
#endif
#include <IotWebConf.h>

namespace iotwebconf
{

class IotWebConfApi
{
public:
  IotWebConfApi(IotWebConf* iotWebConf, WebServer* server);

  // -- Should be called in setup()
  void setConfigSavedCallback(std::function<void()> func);
  
  // -- Helper to register all API routes at once.
  // -- Example: api.registerRoutes();
  // -- This assumes the server has an 'on' method (which ESP WebServer does)
  void registerRoutes();

  // -- Must be called in loop() to handle async operations
  void loop();

  // -- Handler methods (public so they can be used in lambdas if needed manually)
  void handleApiStatus();
  void handleApiNetworks();
  void handleApiWifiConnect();
  void handleApiGetConfig();
  void handleApiSetConfig();
  void handleApiScan();

private:
  IotWebConf* _iotWebConf;
  WebServer* _server;
  std::function<void()> _configSavedCallback = nullptr;

  // -- Async scan handling
  unsigned long _lastScanTime = 0;
  bool _scanRequested = false;
  
  void sendJson(int code, JsonDocument& doc);
};

} // namespace iotwebconf

#endif
