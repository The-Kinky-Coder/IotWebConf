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

#include <vector>
#include <ArduinoJson.h>
#ifdef ESP8266
# include <ESP8266WiFi.h>
# include <ESP8266WebServer.h>
#elif defined(ESP32)
# include <WiFi.h>
# ifndef IOTWEBCONF_USING_ASYNC
#  include <WebServer.h>
# endif
#endif
#include <IotWebConf.h>

// Include AsyncWebServer support if available
#if defined(ESP_ASYNC_WEB_SERVER_H_) || defined(_ESPAsyncWebServer_H_) || defined(IOTWEBCONF_USING_ASYNC)
# include <ESPAsyncWebServer.h>
# define IOTWEBCONF_API_HAS_ASYNC_SUPPORT
#endif

// ═══════════════════════════════════════════════════════
// PHASE 3 CONFIGURATION CONSTANTS
// ═══════════════════════════════════════════════════════

// Maximum number of WiFi networks supported (Phase 3)
#ifndef IOTWEBCONF_MAX_WIFI_NETWORKS
# define IOTWEBCONF_MAX_WIFI_NETWORKS 10
#endif

// Connection test timeout in milliseconds (Phase 3)
#ifndef IOTWEBCONF_WIFI_CONNECT_TIMEOUT_MS
# define IOTWEBCONF_WIFI_CONNECT_TIMEOUT_MS 10000
#endif

// ═══════════════════════════════════════════════════════
// AUTOMATIC BACKGROUND WIFI SCANNING
// ═══════════════════════════════════════════════════════

// Automatic WiFi scan interval in milliseconds
// Default: 15 seconds (allows 3 scans within 45-second cache window)
#ifndef IOTWEBCONF_AUTO_SCAN_INTERVAL_MS
# define IOTWEBCONF_AUTO_SCAN_INTERVAL_MS 60000
#endif

// Enable automatic background scanning by default
// Set to false to require manual POST /api/scan calls
#ifndef IOTWEBCONF_AUTO_SCAN_ENABLED_DEFAULT
# define IOTWEBCONF_AUTO_SCAN_ENABLED_DEFAULT true
#endif

// Import AP auto-reconnect constants from settings
#include "IotWebConfSettings.h"

namespace iotwebconf
{

// Forward declarations
class MultipleWifiAddition;

class IotWebConfApi
{
public:
#ifndef IOTWEBCONF_USING_ASYNC
  /**
   * Constructor for synchronous WebServer
   * @param iotWebConf - IotWebConf instance
   * @param server - Synchronous WebServer instance
   */
  IotWebConfApi(IotWebConf* iotWebConf, WebServer* server);
#endif

#ifdef IOTWEBCONF_API_HAS_ASYNC_SUPPORT
  /**
   * Constructor for asynchronous AsyncWebServer
   * @param iotWebConf - IotWebConf instance
   * @param server - AsyncWebServer instance
   */
  IotWebConfApi(IotWebConf* iotWebConf, AsyncWebServer* server);
#endif

  // -- Should be called in setup()
  // -- The callback runs on the request-handler task (not doLoop()'s task);
  // -- see the calling-contract note on IotWebConf::forceApMode().
  void setConfigSavedCallback(std::function<void()> func);
  
  // -- Helper to register all API routes at once.
  // -- Example: api.registerRoutes();
  // -- If constructed with AsyncWebServer, this auto-dispatches to registerRoutesAsync().
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
  void handleApiGetConfig();
  void handleApiSetConfig();
  void handleApiScan();

  /**
   * Enable or disable automatic background WiFi scanning.
   * When enabled (default), IotWebConfApi::loop() automatically triggers
   * WiFi scans at IOTWEBCONF_AUTO_SCAN_INTERVAL_MS intervals.
   * 
   * @param enabled - true to enable automatic scanning, false to disable
   */
  void setAutoScanEnabled(bool enabled) { _autoScanEnabled = enabled; }
  
  /**
   * Check if automatic background WiFi scanning is enabled.
   * 
   * @return true if auto-scanning is enabled, false otherwise
   */
  bool getAutoScanEnabled() const { return _autoScanEnabled; }

  /**
   * Enable or disable automatic reconnection from AP mode.
   * When enabled (default), device automatically attempts to reconnect
   * to configured networks when detected in scan results while in AP mode.
   * Useful for power-cut scenarios where router boots slower than device.
   * 
   * @param enabled - true to enable auto-reconnect, false to disable
   */
  void setApAutoReconnectEnabled(bool enabled) { _apAutoReconnectEnabled = enabled; }
  
  /**
   * Check if automatic reconnection from AP mode is enabled.
   * 
   * @return true if AP auto-reconnect is enabled, false otherwise
   */
  bool getApAutoReconnectEnabled() const { return _apAutoReconnectEnabled; }

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

  // ═══════════════════════════════════════════════════════
  // PHASE 3 NETWORK MANAGEMENT API (REMOVED - HARD FORK)
  // ═══════════════════════════════════════════════════════
  // The following Phase 2 endpoints have been REMOVED in Phase 3:
  //
  // ❌ POST /api/networks/{id} - Update credentials (use POST /api/networks auto-slot)
  //    - handleApiUpdateNetwork() - REMOVED

  // ═══════════════════════════════════════════════════════
  // PHASE 3 NETWORK MANAGEMENT API
  // ═══════════════════════════════════════════════════════

  /**
   * POST /api/networks
   * Add network to next available slot and auto-connect.
   * 
   * Request body: {"ssid": "NewSSID", "password": "NewPassword"}
   * Response: {"status": "connecting", "id": "wifi2", "slot": 1, "ssid": "..."}
   * 
   * ⚠️ WARNING: This method writes to EEPROM immediately.
   * Auto-connects to network after adding.
   */
  void handleApiAddNetwork();

  /**
   * DELETE /api/networks/{id}
   * Remove network from configuration.
   * 
   * Cannot delete currently active network (returns 409 Conflict).
   * ⚠️ WARNING: This method writes to EEPROM immediately.
   */
  void handleApiDeleteNetwork();

private:
  IotWebConf* _iotWebConf;
#ifndef IOTWEBCONF_USING_ASYNC
  WebServer* _server = nullptr;
#endif
  std::function<void()> _configSavedCallback = nullptr;

#ifdef IOTWEBCONF_API_HAS_ASYNC_SUPPORT
  AsyncWebServer* _asyncServer = nullptr;
  bool _isAsync = false;
#endif

  // -- Async scan handling
  unsigned long _lastScanTime = 0;          // Timestamp of last scan (manual or automatic)
  bool _scanRequested = false;              // True when async scan is in progress
  
  // -- Automatic background scanning
  bool _autoScanEnabled = IOTWEBCONF_AUTO_SCAN_ENABLED_DEFAULT;
  unsigned long _lastAutoScanTime = 0 - IOTWEBCONF_AUTO_SCAN_INTERVAL_MS;  // Init for immediate first scan
  
  // -- Automatic reconnection from AP mode
  bool _apAutoReconnectEnabled = IOTWEBCONF_AP_AUTO_RECONNECT_ENABLED;
  unsigned long _lastApReconnectAttempt = 0;

  // ═══════════════════════════════════════════════════════
  // NETWORK CACHE FOR FLICKER-FREE UI
  // ═══════════════════════════════════════════════════════
  
  /**
   * Cached network information to prevent UI flickering.
   * Maintains network list even during scans.
   */
  struct CachedNetwork {
    String ssid;
    int rssi;
    int encryption;
    unsigned long lastSeen;  // millis() timestamp
  };
  
  std::vector<CachedNetwork> _networkCache;
  
  /**
   * Update network cache from completed scan results.
   * Adds new networks and updates lastSeen for existing ones.
   * Removes stale networks not seen for 45+ seconds (3 scans).
   */
  void updateNetworkCache();

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
   * For single WiFi mode (iwcWifi0), returns the WifiParameterGroup.
   * For multiple WiFi mode, returns a ChainedWifiParameterGroup cast to void*
   * since they don't share a common WiFi base class.
   * 
   * @param id Network ID (e.g., "iwcWifi0", "wifi1")
   * @param outIsSingle Set to true if found network is single WiFi mode (WifiParameterGroup*)
   * @return Pointer to network group (cast to void*), or nullptr if not found
   *         Caller must cast to WifiParameterGroup* if outIsSingle=true,
   *         or ChainedWifiParameterGroup* if outIsSingle=false
   */
  void* findNetworkById(const String& id, bool& outIsSingle);

  // ═══════════════════════════════════════════════════════
  // PHASE 3 NETWORK MANAGEMENT - HELPER METHODS
  // ═══════════════════════════════════════════════════════

  /**
   * Find next available network slot (empty SSID).
   * Checks up to IOTWEBCONF_MAX_WIFI_NETWORKS slots.
   * 
   * @return Pointer to available slot (cast to void* for compatibility),
   *         or nullptr if all slots full. Caller must determine if single
   *         (WifiParameterGroup*) or multi (ChainedWifiParameterGroup*).
   */
  void* findAvailableSlot(bool& outIsSingle);

  /**
   * Find an existing network slot by SSID.
   * Used for duplicate detection / upsert semantics in POST /api/networks.
   * Searches all slots (primary + chained).
   * 
   * @param ssid SSID string to search for (case-sensitive)
   * @param outIsSingle Set to true if found in primary slot (WifiParameterGroup*)
   * @return Pointer to matching slot (cast to void*), or nullptr if not found
   */
  void* findNetworkBySsid(const char* ssid, bool& outIsSingle);

  /**
   * Count configured networks (non-empty SSID).
   * 
   * @return Number of configured networks (0 to IOTWEBCONF_MAX_WIFI_NETWORKS)
   */
  int getConfiguredNetworkCount();

  /**
   * Check if a network is currently active/connected.
   * 
   * @param id Network ID to check (e.g., "iwcWifi0", "wifi1")
   * @return true if this network is currently connected
   */
  bool isNetworkActive(const String& id);

  /**
   * Get the current slot number for a network.
   * Useful for response generation.
   * 
   * @param id Network ID
   * @return Slot number (0-based), or -1 if not found
   */
  int getNetworkSlot(const String& id);

  /**
   * Check if any configured network SSID is currently available in scan cache.
   * Used for automatic reconnection from AP mode (power-cut recovery).
   * 
   * @return true if any configured network is visible in recent scans
   */
  bool isConfiguredNetworkAvailable();

  // ═══════════════════════════════════════════════════════
  // DEFERRED CONNECT STATE MACHINE — private members
  // ═══════════════════════════════════════════════════════

  // Credentials snapshot before switching (RAM only, not persisted)
  String _fallbackSsid;
  String _fallbackPassword;

  // Target of the current connect attempt
  String _pendingConnectSsid;
  String _pendingConnectId;

  // Stage 0: deferred plain reconnect (armed by handleApiReconnect()), separate from
  // _pendingConnect so it never arms _monitoringConnect/the fallback machinery below (#14).
  bool          _pendingReconnect   = false;
  unsigned long _pendingReconnectMs = 0;

  // Stage 1: waiting to fire forceWifiConnection() (~300ms after 202 sent)
  bool          _pendingConnect   = false;
  unsigned long _pendingConnectMs = 0;

  // Stage 2: watching IotWebConf state for success or 30s timeout
  bool          _monitoringConnect = false;
  unsigned long _connectStartMs    = 0;

  // Stage 3: fallback — restoring old credentials and reconnecting (20s)
  bool          _awaitingFallback = false;
  unsigned long _fallbackStartMs  = 0;

  // Last connect result — exposed via GET /api/status
  String        _lastConnectSsid;
  String        _lastConnectResult;     // "ok" | "failed" | "fallback_ok" | "ap_mode"
  unsigned long _lastConnectTimestamp = 0;
};

} // namespace iotwebconf

#endif // IOTWEBCONF_ENABLE_JSON

#endif
