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

#ifdef IOTWEBCONF_ENABLE_JSON

#include <ArduinoJson.h>
#ifdef ESP8266
# include <ESP8266WiFi.h>
# include <ESP8266WebServer.h>
#elif defined(ESP32)
# include <WiFi.h>
# include <WebServer.h>
#endif
#include <IotWebConf.h>

// Include AsyncWebServer support if available
#if defined(ESP_ASYNC_WEB_SERVER_H_) || defined(_ESPAsyncWebServer_H_)
# include <ESPAsyncWebServer.h>
# define IOTWEBCONF_API_HAS_ASYNC_SUPPORT
#endif

namespace iotwebconf
{

// Forward declarations
class MultipleWifiAddition;

class IotWebConfApi
{
public:
  /**
   * Constructor for synchronous WebServer
   * @param iotWebConf - IotWebConf instance
   * @param server - Synchronous WebServer instance
   */
  IotWebConfApi(IotWebConf* iotWebConf, WebServer* server);

#ifdef IOTWEBCONF_API_HAS_ASYNC_SUPPORT
  /**
   * Constructor for asynchronous AsyncWebServer
   * @param iotWebConf - IotWebConf instance
   * @param server - AsyncWebServer instance
   */
  IotWebConfApi(IotWebConf* iotWebConf, AsyncWebServer* server);
#endif

  // -- Should be called in setup()
  void setConfigSavedCallback(std::function<void()> func);
  
  // -- Helper to register all API routes at once.
  // -- Example: api.registerRoutes();
  // -- This assumes the server has an 'on' method (which ESP WebServer does)
  void registerRoutes();

#ifdef IOTWEBCONF_API_HAS_ASYNC_SUPPORT
  /**
   * Register all API routes for AsyncWebServer
   * This overload handles path parameters for async endpoints
   */
  void registerRoutesAsync();
#endif

  // -- Must be called in loop() to handle async operations
  void loop();

  // -- Handler methods (public so they can be used in lambdas if needed manually)
  void handleApiStatus();
  void handleApiNetworks();
  void handleApiWifiConnect();
  void handleApiGetConfig();
  void handleApiSetConfig();
  void handleApiScan();

  // ═══════════════════════════════════════════════════════
  // PHASE 2 NETWORK MANAGEMENT API
  // ═══════════════════════════════════════════════════════

  /**
   * Set the MultipleWifiAddition instance if using multiple WiFi networks.
   * This enables the API to manage multiple network configurations.
   * Optional - only needed if MultipleWifiAddition is used.
   * 
   * @param multiWifi Pointer to MultipleWifiAddition instance (can be nullptr)
   */
  void setMultipleWifiAddition(class MultipleWifiAddition* multiWifi);

  /**
   * GET /api/networks/configured
   * List all configured network credentials.
   * 
   * Response format:
   * {
   *   "networks": [
   *     {"id": "iwcWifi0", "ssid": "HomeNet", "isActive": true, "hasPassword": true, "isCurrent": true},
   *     {"id": "wifi1", "ssid": "OfficeNet", "isActive": true, "hasPassword": true, "isCurrent": false}
   *   ]
   * }
   */
  void handleApiGetConfiguredNetworks();

  /**
   * POST /api/networks/{id}
   * Update credentials for a specific network slot.
   * 
   * Request body: {"ssid": "NewSSID", "password": "NewPassword"}
   * 
   * ⚠️ WARNING: This method writes to EEPROM immediately.
   * EEPROM has limited write cycles (~100,000). Avoid calling repeatedly.
   */
  void handleApiUpdateNetwork();

  /**
   * POST /api/networks/{id}/connect
   * Connect to a specific saved network.
   * 
   * Copies the specified network credentials to primary WiFi slot
   * and triggers a connection attempt via forceWifiConnection().
   */
  void handleApiConnectToNetwork();

  /**
   * POST /api/reconnect
   * Force WiFi reconnection with current settings.
   * 
   * Useful after network changes or connection issues.
   * Does not modify configuration or write to EEPROM.
   */
  void handleApiReconnect();

private:
  IotWebConf* _iotWebConf;
  WebServer* _server;
  std::function<void()> _configSavedCallback = nullptr;

#ifdef IOTWEBCONF_API_HAS_ASYNC_SUPPORT
  AsyncWebServer* _asyncServer = nullptr;
  bool _isAsync = false;
#endif

  // -- Async scan handling
  unsigned long _lastScanTime = 0;
  bool _scanRequested = false;

  // ═══════════════════════════════════════════════════════
  // PHASE 2 NETWORK MANAGEMENT - PRIVATE MEMBERS
  // ═══════════════════════════════════════════════════════
  
  /**
   * Optional reference to MultipleWifiAddition.
   * Set via setMultipleWifiAddition() if user is using multiple WiFi networks.
   * Can be nullptr (single WiFi mode only).
   */
  class MultipleWifiAddition* _multipleWifi = nullptr;
  
  void sendJson(int code, JsonDocument& doc);

#ifdef IOTWEBCONF_API_HAS_ASYNC_SUPPORT
  /**
   * Send JSON response for AsyncWebServer
   */
  void sendJsonAsync(AsyncWebServerRequest* request, int code, JsonDocument& doc);
#endif

  // ═══════════════════════════════════════════════════════
  // PHASE 2 NETWORK MANAGEMENT - HELPER METHODS
  // ═══════════════════════════════════════════════════════

  /**
   * Extract network ID from URL path.
   * Examples:
   *   - "/api/networks/iwcWifi0" -> "iwcWifi0"
   *   - "/api/networks/iwcWifi0/connect" -> "iwcWifi0"
   *   - "/api/networks/home" -> "home"
   * 
   * @param uri Full request URI
   * @param prefix Expected prefix (e.g., "/api/networks/")
   * @return Network ID, or empty string if not found
   */
  String extractNetworkId(const String& uri, const String& prefix);

  /**
   * Find a WiFi parameter group by ID.
   * Searches both single WiFi mode (iwcWifi0) and multiple WiFi mode.
   * 
   * @param id Network ID (e.g., "iwcWifi0", "wifi1")
   * @return Pointer to WifiParameterGroup, or nullptr if not found
   */
  WifiParameterGroup* findNetworkById(const String& id);
};

} // namespace iotwebconf

#endif // IOTWEBCONF_ENABLE_JSON

#endif
