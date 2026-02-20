/**
 * IotWebConfApi.cpp -- JSON API for IotWebConf
 */

#include "IotWebConfApi.h"
#include "IotWebConfMultipleWifi.h"

#ifdef IOTWEBCONF_ENABLE_JSON

namespace iotwebconf
{

#ifndef IOTWEBCONF_USING_ASYNC
IotWebConfApi::IotWebConfApi(IotWebConf* iotWebConf, WebServer* server)
{
  _iotWebConf = iotWebConf;
  _server = server;
#ifdef IOTWEBCONF_API_HAS_ASYNC_SUPPORT
  _isAsync = false;
#endif
}
#endif

#ifdef IOTWEBCONF_API_HAS_ASYNC_SUPPORT
IotWebConfApi::IotWebConfApi(IotWebConf* iotWebConf, AsyncWebServer* server)
{
  _iotWebConf = iotWebConf;
  _asyncServer = server;
  _isAsync = true;
}
#endif

void IotWebConfApi::setConfigSavedCallback(std::function<void()> func)
{
  _configSavedCallback = func;
}

void IotWebConfApi::registerRoutes()
{
#ifdef IOTWEBCONF_API_HAS_ASYNC_SUPPORT
  if (_isAsync && (_asyncServer != nullptr))
  {
    registerRoutesAsync();
    return;
  }
#endif
#ifndef IOTWEBCONF_USING_ASYNC
  if (_server == nullptr)
  {
    return;
  }

  // ═══════════════════════════════════════════════════════
  // PHASE 3 JSON API - 10 ENDPOINTS TOTAL
  // ═══════════════════════════════════════════════════════
  
  // 1. GET /api/status - Device status
  _server->on("/api/status", HTTP_GET, [this]() { this->handleApiStatus(); });
  
  // 2. POST /api/scan - Start WiFi scan
  _server->on("/api/scan", HTTP_POST, [this]() { this->handleApiScan(); });
  
  // 3. GET /api/networks/configured - List saved networks (MUST be before /api/networks)
  _server->on("/api/networks/configured", HTTP_GET, [this]() {
    this->handleApiGetConfiguredNetworks();
  });
  
  // 4. GET /api/networks - Scan results (more general route, registered after specific)
  _server->on("/api/networks", HTTP_GET, [this]() { this->handleApiNetworks(); });
  
  // 5. POST /api/networks - Add network (auto-slot) [PHASE 3 NEW]
  _server->on("/api/networks", HTTP_POST, [this]() {
    this->handleApiAddNetwork();
  });
  
  // 6. DELETE /api/networks/{id} - Remove network [PHASE 3 NEW]
  // Note: WebServer doesn't support path parameters natively
  // Handler extracts ID from URL manually
  
  // 7. POST /api/networks/{id}/connect - Connect to saved network
  // Note: These two endpoints require custom URL parsing in handlers
  
  // 8. POST /api/reconnect - Force reconnection
  _server->on("/api/reconnect", HTTP_POST, [this]() {
    this->handleApiReconnect();
  });
  
  // 9. GET /api/config - Get configuration
  _server->on("/api/config", HTTP_GET, [this]() { this->handleApiGetConfig(); });
  
  // 10. POST /api/config - Update configuration
  _server->on("/api/config", HTTP_POST, [this]() { this->handleApiSetConfig(); });

  // ═══════════════════════════════════════════════════════
  // LEGACY REMOVED (Phase 3 Hard Fork):
  // - POST /api/wifi (redundant with POST /api/networks)
  // - POST /api/networks/{id} (manual slot update - use auto-slot instead)
  // ═══════════════════════════════════════════════════════
#endif // IOTWEBCONF_USING_ASYNC
}

void IotWebConfApi::loop()
{
  // -- Handle async scan results and update cache
  if (_scanRequested)
  {
    int n = WiFi.scanComplete();
    if (n >= 0)
    {
      _scanRequested = false;
      // Update cache with scan results
      updateNetworkCache();
    }
    else if (n == -2)
    {
      // Scan failed
      _scanRequested = false;
    }
  }
  
  // -- Automatic background WiFi scanning
  // Trigger periodic scans to keep network cache fresh
  if (_autoScanEnabled && !_scanRequested)
  {
    unsigned long now = millis();
    
    // Guard: Don't auto-scan during initial connection attempts
    // Allow scanning when: connected, in AP mode (so users see network list), or after 30s
    bool canAutoScan = (_iotWebConf->getState() == iotwebconf::OnLine) || 
                       (_iotWebConf->getState() == iotwebconf::ApMode) ||
                       (now > 30000);
    
    // Check if enough time has passed since last automatic scan
    if (canAutoScan && (now - _lastAutoScanTime) >= IOTWEBCONF_AUTO_SCAN_INTERVAL_MS)
    {
      // Trigger async scan (non-blocking)
      WiFi.scanNetworks(true); // true = async
      _scanRequested = true;
      _lastScanTime = now;
      _lastAutoScanTime = now;
      
      IOTWEBCONF_DEBUG_LINE(F("API: Automatic WiFi scan triggered"));
    }
  }
  
  // -- Automatic reconnection from AP mode when network becomes available
  // Handles power-cut scenarios where router boots slower than device
  if (_apAutoReconnectEnabled && 
      (_iotWebConf->getState() == iotwebconf::ApMode))
  {
    unsigned long now = millis();
    
    // Respect minimum interval between attempts (avoid bouncing)
    if ((now - _lastApReconnectAttempt) >= IOTWEBCONF_AP_AUTO_RECONNECT_INTERVAL_MS)
    {
      // Check if any configured network is now available
      if (isConfiguredNetworkAvailable())
      {
        _lastApReconnectAttempt = now;
        
        IOTWEBCONF_DEBUG_LINE(F("API: Configured network detected, triggering reconnection"));
        
        // Trigger reconnection attempt
        _iotWebConf->forceWifiConnection();
      }
    }
  }
}

void IotWebConfApi::updateNetworkCache()
{
  int n = WiFi.scanComplete();
  if (n < 0)
  {
    return; // Scan not complete or failed
  }
  
  unsigned long now = millis();
  
  // Update/add networks from scan results
  for (int i = 0; i < n; i++)
  {
    String ssid = WiFi.SSID(i);
    int rssi = WiFi.RSSI(i);
    int encryption = WiFi.encryptionType(i);
    
    // Find existing network in cache
    bool found = false;
    for (auto& cachedNet : _networkCache)
    {
      if (cachedNet.ssid == ssid)
      {
        // Update existing network
        cachedNet.rssi = rssi;
        cachedNet.encryption = encryption;
        cachedNet.lastSeen = now;
        found = true;
        break;
      }
    }
    
    // Add new network if not found
    if (!found)
    {
      CachedNetwork newNet;
      newNet.ssid = ssid;
      newNet.rssi = rssi;
      newNet.encryption = encryption;
      newNet.lastSeen = now;
      _networkCache.push_back(newNet);
    }
  }
  
  // Remove stale networks (not seen for 45+ seconds = 3 scans at 15s interval)
  _networkCache.erase(
    std::remove_if(_networkCache.begin(), _networkCache.end(),
      [now](const CachedNetwork& net) {
        return (now - net.lastSeen) > 45000; // 45 seconds
      }),
    _networkCache.end()
  );
  
  // Clear ESP scan results (free memory)
  WiFi.scanDelete();
}

void IotWebConfApi::handleApiStatus()
{
  JsonDocument doc;
  
  doc["state"] = _iotWebConf->getState(); // Enum value
  doc["thingName"] = _iotWebConf->getThingName();
  doc["ip"] = WiFi.localIP().toString();
  doc["ip_v4"] = WiFi.localIP().toString();
#if defined(ESP32) && defined(CONFIG_LWIP_IPV6)
  String ipv6Global = WiFi.globalIPv6().toString();
  String ipv6LinkLocal = WiFi.linkLocalIPv6().toString();
  if (ipv6Global.length() > 0)
  {
    doc["ip_v6"] = ipv6Global;
    doc["ip_v6_ll"] = false;
  }
  else if (ipv6LinkLocal.length() > 0)
  {
    doc["ip_v6"] = ipv6LinkLocal;
    doc["ip_v6_ll"] = true;
  }
#endif
  doc["mac"] = WiFi.macAddress();
  
  // Wifi connection status
  doc["wifi"]["status"] = WiFi.status();
  doc["wifi"]["ssid"] = WiFi.SSID();
  doc["wifi"]["rssi"] = WiFi.RSSI();
  
  // AP Status
  doc["ap"]["active"] = (WiFi.getMode() & WIFI_AP);
  doc["ap"]["clients"] = WiFi.softAPgetStationNum();
  doc["ap"]["ip"] = WiFi.softAPIP().toString();

  sendJson(200, doc);
}

void IotWebConfApi::handleApiNetworks()
{
  
  // Always return cached results (never empty!)
  // This prevents UI flickering during scans
  
  JsonDocument doc;
  JsonArray networks = doc["networks"].to<JsonArray>();
  
  // Add all cached networks
  for (const auto& net : _networkCache)
  {
    JsonObject obj = networks.add<JsonObject>();
    obj["ssid"] = net.ssid;
    obj["rssi"] = net.rssi;
    obj["encryption"] = net.encryption;
    obj["lastSeen"] = (millis() - net.lastSeen) / 1000; // seconds ago
  }
  
  // Include scanning status
  int scanStatus = WiFi.scanComplete();
  doc["scanning"] = (scanStatus == -1); // -1 = scan in progress
  
  sendJson(200, doc);
}

void IotWebConfApi::handleApiScan()
{
  // Trigger async scan
  WiFi.scanNetworks(true); // true = async
  _scanRequested = true;
  _lastScanTime = millis();
  _lastAutoScanTime = millis(); // Reset auto-scan timer to prevent immediate auto-scan
  
  JsonDocument doc;
  doc["status"] = "Scan started";
  sendJson(202, doc);
}

// ═══════════════════════════════════════════════════════
// REMOVED IN PHASE 3 (Hard Fork)
// ═══════════════════════════════════════════════════════
// The following handler was removed as POST /api/wifi is redundant
// with the new POST /api/networks (auto-slot) endpoint.
//
// void IotWebConfApi::handleApiWifiConnect() - REMOVED

void IotWebConfApi::handleApiGetConfig()
{
  // Serialize all parameters to JSON
  // We can use the load/saveToJson methods if IOTWEBCONF_ENABLE_JSON is defined
  
  JsonDocument doc;
  JsonObject root = doc.to<JsonObject>();
  
  ParameterGroup* group = _iotWebConf->getRootParameterGroup();
  group->saveToJson(root);
  
  sendJson(200, doc);
}

#ifndef IOTWEBCONF_USING_ASYNC
void IotWebConfApi::handleApiSetConfig()
{
  if (!_server->hasArg("plain")) 
  {
    JsonDocument doc;
    doc["error"] = "Body missing";
    sendJson(400, doc);
    return;
  }

  String body = _server->arg("plain");
  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, body);

  if (error) 
  {
    JsonDocument errDoc;
    errDoc["error"] = "Invalid JSON";
    sendJson(400, errDoc);
    return;
  }

  JsonObject root = doc.as<JsonObject>();
  _iotWebConf->getRootParameterGroup()->loadFromJson(root);
  
  // Save to EEPROM
  _iotWebConf->saveConfig();
  
  if (_configSavedCallback) {
      _configSavedCallback();
  }

  JsonDocument resDoc;
  resDoc["status"] = "Saved";
  sendJson(200, resDoc);
}
#endif // IOTWEBCONF_USING_ASYNC

#ifndef IOTWEBCONF_USING_ASYNC
void IotWebConfApi::sendJson(int code, JsonDocument& doc)
{
  String response;
  serializeJson(doc, response);
  _server->sendHeader("Access-Control-Allow-Origin", "*");
  _server->send(code, "application/json", response);
}
#endif // IOTWEBCONF_USING_ASYNC

#ifdef IOTWEBCONF_API_HAS_ASYNC_SUPPORT
void IotWebConfApi::sendJsonAsync(AsyncWebServerRequest* request, int code, JsonDocument& doc)
{
  String response;
  serializeJson(doc, response);
  
  AsyncWebServerResponse* asyncResponse = request->beginResponse(code, "application/json", response);
  asyncResponse->addHeader("Access-Control-Allow-Origin", "*");
  request->send(asyncResponse);
}
#endif

// ═══════════════════════════════════════════════════════
// PHASE 2 NETWORK MANAGEMENT API - IMPLEMENTATION
// ═══════════════════════════════════════════════════════

void IotWebConfApi::setMultipleWifiAddition(MultipleWifiAddition* multiWifi)
{
  _multipleWifi = multiWifi;
}

// ═══════════════════════════════════════════════════════
// PHASE 3 HELPER METHODS
// ═══════════════════════════════════════════════════════

void* IotWebConfApi::findAvailableSlot(bool& outIsSingle)
{
  // iwcWifi0 (primary slot) is an internal IotWebConf slot — not user-managed.
  // Only allocate from chained slots (wifi1, wifi2, ...).
  int slotCount = 0;

  if (_multipleWifi != nullptr)
  {
    ChainedWifiParameterGroup* current = _multipleWifi->getFirstSet();
    while (current != nullptr && slotCount < IOTWEBCONF_MAX_WIFI_NETWORKS)
    {
      if (strlen(current->wifiSsid) == 0)
      {
        outIsSingle = false;
        return (void*)current;
      }
      slotCount++;
      current = (ChainedWifiParameterGroup*)current->getNext();
    }
  }

  outIsSingle = false;
  return nullptr;
}

void* IotWebConfApi::findNetworkBySsid(const char* ssid, bool& outIsSingle)
{
  if (!ssid || strlen(ssid) == 0)
  {
    outIsSingle = false;
    return nullptr;
  }

  // Only search chained slots — iwcWifi0 is internal and not user-managed.
  if (_multipleWifi != nullptr)
  {
    ChainedWifiParameterGroup* current = _multipleWifi->getFirstSet();
    while (current != nullptr)
    {
      if (strlen(current->wifiSsid) > 0 && strcmp(current->wifiSsid, ssid) == 0)
      {
        outIsSingle = false;
        return (void*)current;
      }
      current = (ChainedWifiParameterGroup*)current->getNext();
    }
  }

  outIsSingle = false;
  return nullptr;
}

int IotWebConfApi::getConfiguredNetworkCount()
{
  // Only count chained slots — iwcWifi0 is internal and not user-managed.
  int count = 0;

  if (_multipleWifi != nullptr)
  {
    ChainedWifiParameterGroup* current = _multipleWifi->getFirstSet();
    while (current != nullptr)
    {
      if (strlen(current->wifiSsid) > 0)
      {
        count++;
      }
      current = (ChainedWifiParameterGroup*)current->getNext();
    }
  }

  return count;
}

bool IotWebConfApi::isNetworkActive(const String& id)
{
  // Get currently connected SSID
  String currentSsid = WiFi.SSID();
  
  if (currentSsid.length() == 0)
  {
    // Not connected to any network
    return false;
  }
  
  // Find the network and check if its SSID matches current connection.
  // findNetworkById only returns chained slots, so isSingle is always false.
  bool isSingle = false;
  void* networkPtr = findNetworkById(id, isSingle);

  if (networkPtr == nullptr)
  {
    return false;
  }

  ChainedWifiParameterGroup* network = (ChainedWifiParameterGroup*)networkPtr;
  return (String(network->wifiSsid) == currentSsid);
}

int IotWebConfApi::getNetworkSlot(const String& id)
{
  int slot = 0;
  
  // 1. Check primary WiFi slot (iwcWifi0)
  WifiParameterGroup* primaryWifi = _iotWebConf->getWifiParameterGroup();
  if (id == primaryWifi->getId())
  {
    return 0;
  }
  slot++;
  
  // 2. Check additional network slots
  if (_multipleWifi != nullptr)
  {
    ChainedWifiParameterGroup* current = _multipleWifi->getFirstSet();
    while (current != nullptr)
    {
      if (id == current->getId())
      {
        return slot;
      }
      slot++;
      current = (ChainedWifiParameterGroup*)current->getNext();
    }
  }
  
  return -1; // Not found
}


String IotWebConfApi::extractNetworkId(const String& uri, const String& prefix)
{
  // Extract network ID from URI path
  // Example: "/api/networks/iwcWifi0/connect" -> "iwcWifi0"
  //          "/api/networks/home" -> "home"
  
  if (!uri.startsWith(prefix))
  {
    return "";
  }
  
  int idStart = prefix.length();
  int idEnd = uri.indexOf('/', idStart);
  
  if (idEnd == -1)
  {
    // No trailing slash, ID goes to end of string
    idEnd = uri.length();
  }
  
  return uri.substring(idStart, idEnd);
}

void* IotWebConfApi::findNetworkById(const String& id, bool& outIsSingle)
{
  // iwcWifi0 is an internal IotWebConf slot — reject any direct API access to it.
  // All user-facing operations target chained slots (wifi1, wifi2, ...) only.
  WifiParameterGroup* primaryWifi = _iotWebConf->getWifiParameterGroup();
  if (id == primaryWifi->getId())
  {
    outIsSingle = false;
    return nullptr; // Blocked — not user-addressable
  }

  if (_multipleWifi != nullptr)
  {
    ChainedWifiParameterGroup* current = _multipleWifi->getFirstSet();
    while (current != nullptr)
    {
      if (id == current->getId())
      {
        outIsSingle = false;
        return (void*)current;
      }
      current = (ChainedWifiParameterGroup*)current->getNext();
    }
  }

  outIsSingle = false;
  return nullptr;
}

void IotWebConfApi::handleApiGetConfiguredNetworks()
{
  JsonDocument doc;
  JsonArray networks = doc["networks"].to<JsonArray>();

  // iwcWifi0 is an internal IotWebConf slot — not returned to API consumers.
  // Only chained slots (wifi1, wifi2, ...) are user-managed.
  String currentSsid = WiFi.SSID();

  if (_multipleWifi != nullptr)
  {
    ChainedWifiParameterGroup* current = _multipleWifi->getFirstSet();
    while (current != nullptr)
    {
      JsonObject net = networks.add<JsonObject>();
      net["id"] = current->getId();
      net["ssid"] = current->wifiSsid;
      net["isActive"] = current->isActive();
      net["hasPassword"] = (strlen(current->wifiPassword) > 0);
      net["isCurrent"] = (String(current->wifiSsid) == currentSsid);

      current = (ChainedWifiParameterGroup*)current->getNext();
    }
  }

  sendJson(200, doc);
}

// ═══════════════════════════════════════════════════════
// REMOVED IN PHASE 3 (Hard Fork)
// ═══════════════════════════════════════════════════════
// The following handler was removed as POST /api/networks/{id}
// (manual slot update) is replaced by POST /api/networks (auto-slot).
//
// void IotWebConfApi::handleApiUpdateNetwork() - REMOVED

#ifndef IOTWEBCONF_USING_ASYNC
void IotWebConfApi::handleApiConnectToNetwork()
{
  // 1. Extract network ID from URL
  String uri = _server->uri();
  String networkId = extractNetworkId(uri, "/api/networks/");

  if (networkId.length() == 0)
  {
    JsonDocument doc;
    doc["error"] = "Network ID missing in URL";
    sendJson(400, doc);
    return;
  }

  // 2. Find the network
  bool isSingle = false;
  void* networkPtr = findNetworkById(networkId, isSingle);
  if (networkPtr == nullptr)
  {
    JsonDocument doc;
    doc["error"] = "Network not found";
    sendJson(404, doc);
    return;
  }

  // 3. Check if network has credentials and extract them.
  // findNetworkById only returns chained slots.
  ChainedWifiParameterGroup* chainedNet = (ChainedWifiParameterGroup*)networkPtr;
  const char* networkSsid     = chainedNet->wifiSsid;
  const char* networkPassword = chainedNet->wifiPassword;

  if (strlen(networkSsid) == 0)
  {
    JsonDocument doc;
    doc["error"] = "Network not configured (SSID empty)";
    sendJson(400, doc);
    return;
  }

  // 4. Copy credentials to primary WiFi parameters
  //    (IotWebConf uses the primary WiFi slot for connections)
  WifiParameterGroup* primaryWifi = _iotWebConf->getWifiParameterGroup();
  strncpy(primaryWifi->_wifiSsid, networkSsid, IOTWEBCONF_WORD_LEN);
  primaryWifi->_wifiSsid[IOTWEBCONF_WORD_LEN - 1] = '\0';
  
  strncpy(primaryWifi->_wifiPassword, networkPassword, IOTWEBCONF_PASSWORD_LEN);
  primaryWifi->_wifiPassword[IOTWEBCONF_PASSWORD_LEN - 1] = '\0';

  // 5. Save config (so it persists across reboots)
  // ⚠️ WARNING: This writes to EEPROM
  _iotWebConf->saveConfig();

  // 6. Return success BEFORE triggering WiFi reconnection
  // ⚠️ CRITICAL: Response must be sent before WiFi state change
  JsonDocument doc;
  doc["status"] = "connecting";
  doc["id"] = networkId;
  doc["ssid"] = networkSsid;
  sendJson(200, doc);
  
  // Give the HTTP response time to be transmitted before network changes
  delay(100);

  // 7. Trigger connection attempt (this changes WiFi state and may disconnect)
  _iotWebConf->forceWifiConnection();
}

void IotWebConfApi::handleApiReconnect()
{
  // Force WiFi reconnection with current settings
  // This does NOT modify config or write to EEPROM
  
  // ⚠️ CRITICAL: Send response BEFORE triggering WiFi reconnection
  JsonDocument doc;
  doc["status"] = "reconnecting";
  sendJson(200, doc);
  
  // Give the HTTP response time to be transmitted before network changes
  delay(100);
  
  // Now trigger reconnection (this changes WiFi state and may disconnect)
  _iotWebConf->forceWifiConnection();
}

// ═══════════════════════════════════════════════════════
// PHASE 3 NETWORK MANAGEMENT API - SYNCHRONOUS HANDLERS
// ═══════════════════════════════════════════════════════

void IotWebConfApi::handleApiAddNetwork()
{
  // 1. Parse request body
  if (!_server->hasArg("plain"))
  {
    JsonDocument doc;
    doc["error"] = "Request body missing";
    doc["status"] = "error";
    sendJson(400, doc);
    return;
  }

  String body = _server->arg("plain");
  JsonDocument reqDoc;
  DeserializationError error = deserializeJson(reqDoc, body);

  if (error)
  {
    JsonDocument doc;
    doc["error"] = "Invalid JSON";
    doc["status"] = "error";
    sendJson(400, doc);
    return;
  }

  // 2. Validate required fields
  const char* ssid = reqDoc["ssid"];
  const char* password = reqDoc["password"];

  if (!ssid || strlen(ssid) == 0)
  {
    JsonDocument doc;
    doc["error"] = "SSID field required";
    doc["status"] = "error";
    sendJson(400, doc);
    return;
  }

  // 3. Upsert: check if a slot with this SSID already exists
  bool isSingle = false;
  void* slotPtr = findNetworkBySsid(ssid, isSingle);
  bool isUpdate = (slotPtr != nullptr);

  if (!isUpdate)
  {
    // No existing slot — find the next empty one
    slotPtr = findAvailableSlot(isSingle);
    if (slotPtr == nullptr)
    {
      JsonDocument doc;
      doc["error"] = "Maximum networks reached";
      doc["status"] = "error";
      sendJson(507, doc); // 507 Insufficient Storage
      return;
    }
  }

  // 4. Update slot with credentials.
  // findNetworkBySsid/findAvailableSlot only return chained slots.
  ChainedWifiParameterGroup* chainedSlot = (ChainedWifiParameterGroup*)slotPtr;
  strncpy(chainedSlot->wifiSsid, ssid, IOTWEBCONF_WORD_LEN);
  chainedSlot->wifiSsid[IOTWEBCONF_WORD_LEN - 1] = '\0';

  if (password)
  {
    strncpy(chainedSlot->wifiPassword, password, IOTWEBCONF_PASSWORD_LEN);
    chainedSlot->wifiPassword[IOTWEBCONF_PASSWORD_LEN - 1] = '\0';
  }
  else
  {
    chainedSlot->wifiPassword[0] = '\0';
  }

  String networkId = chainedSlot->getId();

  // 5. If this SSID matches iwcWifi0 (the currently active internal slot),
  //    keep its password in sync so the next forceWifiConnection() uses the
  //    updated credentials (e.g. user updating password of the active network).
  WifiParameterGroup* primaryWifi = _iotWebConf->getWifiParameterGroup();
  if (String(primaryWifi->_wifiSsid) == String(ssid) && password)
  {
    strncpy(primaryWifi->_wifiPassword, password, IOTWEBCONF_PASSWORD_LEN);
    primaryWifi->_wifiPassword[IOTWEBCONF_PASSWORD_LEN - 1] = '\0';
  }

  // 6. Save configuration to EEPROM
  _iotWebConf->saveConfig();

  // 6. Build and send response
  int slot = getNetworkSlot(networkId);
  JsonDocument resDoc;
  resDoc["status"] = "connecting";
  resDoc["id"] = networkId;
  resDoc["slot"] = slot;
  resDoc["ssid"] = ssid;
  resDoc["updated"] = isUpdate;  // true = password updated in existing slot, false = new slot
  sendJson(200, resDoc);

  // 7. Allow transmission before WiFi change
  delay(100);

  // 8. Trigger connection test
  _iotWebConf->forceWifiConnection();
}

void IotWebConfApi::handleApiDeleteNetwork()
{
  // 1. Extract network ID from URL
  String uri = _server->uri();
  String networkId = extractNetworkId(uri, "/api/networks/");

  if (networkId.length() == 0)
  {
    JsonDocument doc;
    doc["error"] = "Network ID missing in URL";
    doc["status"] = "error";
    sendJson(400, doc);
    return;
  }

  // 2. Find the network
  bool isSingle = false;
  void* networkPtr = findNetworkById(networkId, isSingle);
  
  if (networkPtr == nullptr)
  {
    JsonDocument doc;
    doc["error"] = "Network not found";
    doc["status"] = "error";
    sendJson(404, doc);
    return;
  }

  // 3. Check if network is currently active
  if (isNetworkActive(networkId))
  {
    JsonDocument doc;
    doc["error"] = "Cannot delete active network";
    doc["status"] = "error";
    doc["id"] = networkId;
    sendJson(409, doc); // 409 Conflict
    return;
  }

  // 4. Clear network credentials
  // findNetworkById only returns chained slots, so isSingle is always false here.
  ChainedWifiParameterGroup* network = (ChainedWifiParameterGroup*)networkPtr;
  String deletedSsid = String(network->wifiSsid);
  network->wifiSsid[0] = '\0';
  network->wifiPassword[0] = '\0';

  // 5. If the deleted SSID matches iwcWifi0 (the active slot), clear that too.
  //    iwcWifi0 mirrors the last-connected network; keeping stale credentials there
  //    would cause IotWebConf to attempt reconnection to a deleted network on reboot.
  WifiParameterGroup* primaryWifi = _iotWebConf->getWifiParameterGroup();
  if (deletedSsid.length() > 0 && deletedSsid == String(primaryWifi->_wifiSsid))
  {
    primaryWifi->_wifiSsid[0] = '\0';
    primaryWifi->_wifiPassword[0] = '\0';
  }

  // 6. Save to EEPROM
  _iotWebConf->saveConfig();

  // 7. Return success
  JsonDocument resDoc;
  resDoc["status"] = "deleted";
  resDoc["id"] = networkId;
  sendJson(200, resDoc);
}
#endif // IOTWEBCONF_USING_ASYNC

bool IotWebConfApi::isConfiguredNetworkAvailable()
{
  // Check if any configured SSID appears in recent scan results
  // Used for automatic reconnection from AP mode (power-cut recovery)
  
  // 1. Get primary WiFi SSID
  WifiParameterGroup* primaryWifi = _iotWebConf->getWifiParameterGroup();
  String primarySsid = String(primaryWifi->_wifiSsid);
  
  // Check if empty (no WiFi configured)
  if (primarySsid.length() == 0)
  {
    return false;
  }
  
  // 2. Search network cache for primary SSID
  for (const auto& net : _networkCache)
  {
    if (net.ssid == primarySsid)
    {
      return true;  // Found!
    }
  }
  
  // 3. Check additional networks from MultipleWifi (if configured)
  if (_multipleWifi != nullptr)
  {
    ChainedWifiParameterGroup* current = _multipleWifi->getFirstSet();
    while (current != nullptr)
    {
      String ssid = String(current->wifiSsid);
      if (ssid.length() > 0)
      {
        for (const auto& net : _networkCache)
        {
          if (net.ssid == ssid)
          {
            return true;  // Found!
          }
        }
      }
      current = (ChainedWifiParameterGroup*)current->getNext();
    }
  }
  
  return false;  // No configured networks found in scan
}

#ifdef IOTWEBCONF_API_HAS_ASYNC_SUPPORT
// ═══════════════════════════════════════════════════════
// ASYNCWEBSERVER ROUTE REGISTRATION
// ═══════════════════════════════════════════════════════

void IotWebConfApi::registerRoutesAsync()
{
  // ═══════════════════════════════════════════════════════
  // PHASE 3 JSON API - 10 ENDPOINTS TOTAL (ASYNC)
  // ═══════════════════════════════════════════════════════
  
  // 1. GET /api/status - Device status
  _asyncServer->on("/api/status", HTTP_GET, [this](AsyncWebServerRequest* request) {
    JsonDocument doc;
    
    doc["state"] = _iotWebConf->getState();
    doc["thingName"] = _iotWebConf->getThingName();
    doc["ip"] = WiFi.localIP().toString();
    doc["ip_v4"] = WiFi.localIP().toString();
#if defined(ESP32) && defined(CONFIG_LWIP_IPV6)
    String ipv6Global = WiFi.globalIPv6().toString();
    String ipv6LinkLocal = WiFi.linkLocalIPv6().toString();
    if (ipv6Global.length() > 0)
    {
      doc["ip_v6"] = ipv6Global;
      doc["ip_v6_ll"] = false;
    }
    else if (ipv6LinkLocal.length() > 0)
    {
      doc["ip_v6"] = ipv6LinkLocal;
      doc["ip_v6_ll"] = true;
    }
#endif
    doc["mac"] = WiFi.macAddress();
    
    doc["wifi"]["status"] = WiFi.status();
    doc["wifi"]["ssid"] = WiFi.SSID();
    doc["wifi"]["rssi"] = WiFi.RSSI();
    
    doc["ap"]["active"] = (WiFi.getMode() & WIFI_AP);
    doc["ap"]["clients"] = WiFi.softAPgetStationNum();
    doc["ap"]["ip"] = WiFi.softAPIP().toString();

    sendJsonAsync(request, 200, doc);
  });

  // 2. POST /api/scan - Start WiFi scan
  _asyncServer->on("/api/scan", HTTP_POST, [this](AsyncWebServerRequest* request) {
    WiFi.scanNetworks(true);
    _scanRequested = true;
    _lastScanTime = millis();
    _lastAutoScanTime = millis();
    
    JsonDocument doc;
    doc["status"] = "Scan started";
    sendJsonAsync(request, 202, doc);
  });

  // 3. GET /api/networks/configured - List saved networks (MUST be before /api/networks)
  _asyncServer->on("/api/networks/configured", HTTP_GET, [this](AsyncWebServerRequest* request) {
    
    JsonDocument doc;
    JsonArray networks = doc["networks"].to<JsonArray>();

    // iwcWifi0 is an internal IotWebConf slot — not returned to API consumers.
    // Only chained slots (wifi1, wifi2, ...) are user-managed.
    String currentSsid = WiFi.SSID();

    if (_multipleWifi != nullptr) {
      ChainedWifiParameterGroup* current = _multipleWifi->getFirstSet();
      while (current != nullptr) {
        JsonObject net = networks.add<JsonObject>();
        net["id"] = current->getId();
        net["ssid"] = current->wifiSsid;
        net["isActive"] = current->isActive();
        net["hasPassword"] = (strlen(current->wifiPassword) > 0);
        net["isCurrent"] = (String(current->wifiSsid) == currentSsid);

        current = (ChainedWifiParameterGroup*)current->getNext();
      }
    }

    sendJsonAsync(request, 200, doc);
  });

  // 4. GET /api/networks - Scan results (more general route, registered after specific)
  _asyncServer->on("/api/networks", HTTP_GET, [this](AsyncWebServerRequest* request) {
    // Always return cached results (never empty!)
    JsonDocument doc;
    JsonArray networks = doc["networks"].to<JsonArray>();
    
    // Add all cached networks
    for (const auto& net : _networkCache)
    {
      JsonObject obj = networks.add<JsonObject>();
      obj["ssid"] = net.ssid;
      obj["rssi"] = net.rssi;
      obj["encryption"] = net.encryption;
      obj["lastSeen"] = (millis() - net.lastSeen) / 1000; // seconds ago
    }
    
    // Include scanning status
    int scanStatus = WiFi.scanComplete();
    doc["scanning"] = (scanStatus == -1); // -1 = scan in progress
    
    sendJsonAsync(request, 200, doc);
  });

  // ═══════════════════════════════════════════════════════
  // DEBUG TEST ENDPOINT — remove after diagnosis
  // Tests: does a plain regex POST (no body handler) fire at all?
  // ═══════════════════════════════════════════════════════
  _asyncServer->on("^\\/api\\/debug\\/regex-test$", HTTP_POST, [this](AsyncWebServerRequest* request) {
    Serial.println("[DEBUG] POST /api/debug/regex-test - onRequest FIRED (3-arg regex POST)");
    JsonDocument doc;
    doc["hit"] = "onRequest";
    doc["method"] = "3-arg";
    sendJsonAsync(request, 200, doc);
  });

  // ═══════════════════════════════════════════════════════
  // DEBUG TEST ENDPOINT 2 — remove after diagnosis
  // Tests: does a 5-arg regex POST fire onRequest?
  // ═══════════════════════════════════════════════════════
  _asyncServer->on("^\\/api\\/debug\\/regex-test-5arg$", HTTP_POST,
    [this](AsyncWebServerRequest* request) {
      Serial.println("[DEBUG] POST /api/debug/regex-test-5arg - onRequest FIRED (5-arg)");
      JsonDocument doc;
      doc["hit"] = "onRequest";
      doc["method"] = "5-arg";
      sendJsonAsync(request, 200, doc);
    },
    nullptr,
    [](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total) {
      Serial.printf("[DEBUG] POST /api/debug/regex-test-5arg - onBody FIRED len=%u index=%u total=%u\n", len, index, total);
    }
  );

  // 5. POST /api/networks - Add network (auto-slot) [PHASE 3 NEW]
  // ⚠️ IMPORTANT: Use regex URL so ESPAsyncWebServer does EXACT matching.
  // Non-regex handlers use prefix matching: "/api/networks" would also match
  // "/api/networks/wifi2/connect", intercepting the connect route below.
  _asyncServer->on("^\\/api\\/networks$", HTTP_POST, [this](AsyncWebServerRequest* request) {
    Serial.println("[DEBUG] POST /api/networks - onRequest FIRED (check: should NOT fire for /connect)");
  }, nullptr,
    [this](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total) {
      Serial.printf("[DEBUG] POST /api/networks - onBody FIRED len=%u index=%u total=%u\n", len, index, total);
      if (index == 0) {
        // Parse request body
        String body = String((char*)data);
        JsonDocument reqDoc;
        DeserializationError error = deserializeJson(reqDoc, body);

        if (error) {
          JsonDocument doc;
          doc["error"] = "Invalid JSON";
          doc["status"] = "error";
          sendJsonAsync(request, 400, doc);
          return;
        }

        const char* ssid = reqDoc["ssid"];
        const char* password = reqDoc["password"];

        if (!ssid || strlen(ssid) == 0) {
          JsonDocument doc;
          doc["error"] = "SSID field required";
          doc["status"] = "error";
          sendJsonAsync(request, 400, doc);
          return;
        }

        // Upsert: check if a slot with this SSID already exists
        bool isSingle = false;
        void* slotPtr = findNetworkBySsid(ssid, isSingle);
        bool isUpdate = (slotPtr != nullptr);

        if (!isUpdate) {
          // No existing slot — find the next empty one
          slotPtr = findAvailableSlot(isSingle);
          if (slotPtr == nullptr) {
            JsonDocument doc;
            doc["error"] = "Maximum networks reached";
            doc["status"] = "error";
            sendJsonAsync(request, 507, doc);
            return;
          }
        }

        // Update slot with credentials.
        // findNetworkBySsid/findAvailableSlot only return chained slots.
        ChainedWifiParameterGroup* chainedSlot = (ChainedWifiParameterGroup*)slotPtr;
        strncpy(chainedSlot->wifiSsid, ssid, IOTWEBCONF_WORD_LEN);
        chainedSlot->wifiSsid[IOTWEBCONF_WORD_LEN - 1] = '\0';

        if (password) {
          strncpy(chainedSlot->wifiPassword, password, IOTWEBCONF_PASSWORD_LEN);
          chainedSlot->wifiPassword[IOTWEBCONF_PASSWORD_LEN - 1] = '\0';
        } else {
          chainedSlot->wifiPassword[0] = '\0';
        }

        String networkId = chainedSlot->getId();

        // If this SSID matches iwcWifi0 (the currently active internal slot),
        // keep its password in sync so forceWifiConnection() uses updated credentials.
        WifiParameterGroup* primaryWifi = _iotWebConf->getWifiParameterGroup();
        if (String(primaryWifi->_wifiSsid) == String(ssid) && password) {
          strncpy(primaryWifi->_wifiPassword, password, IOTWEBCONF_PASSWORD_LEN);
          primaryWifi->_wifiPassword[IOTWEBCONF_PASSWORD_LEN - 1] = '\0';
        }

        // Save configuration
        _iotWebConf->saveConfig();

        // Build and send response
        int slot = getNetworkSlot(networkId);
        JsonDocument resDoc;
        resDoc["status"] = "connecting";
        resDoc["id"] = networkId;
        resDoc["slot"] = slot;
        resDoc["ssid"] = ssid;
        resDoc["updated"] = isUpdate;  // true = password updated in existing slot
        sendJsonAsync(request, 200, resDoc);

        // Allow response transmission before WiFi change
        // 500ms gives the async TCP stack enough time to flush the response
        // before WiFi is disconnected by forceWifiConnection()
        delay(500);

        // Trigger connection test
        _iotWebConf->forceWifiConnection();
      }
    });

  // 6. DELETE /api/networks/{id} - Remove network [PHASE 3 NEW]
  _asyncServer->on("^\\/api\\/networks\\/([^\\/]+)$", HTTP_DELETE, [this](AsyncWebServerRequest* request) {
    // Extract network ID from URL
    String uri = request->url();
    String networkId = extractNetworkId(uri, "/api/networks/");

    if (networkId.length() == 0) {
      JsonDocument doc;
      doc["error"] = "Network ID missing in URL";
      doc["status"] = "error";
      sendJsonAsync(request, 400, doc);
      return;
    }

    // Find the network
    bool isSingle = false;
    void* networkPtr = findNetworkById(networkId, isSingle);
    
    if (networkPtr == nullptr) {
      JsonDocument doc;
      doc["error"] = "Network not found";
      doc["status"] = "error";
      sendJsonAsync(request, 404, doc);
      return;
    }

    // Check if network is currently active
    if (isNetworkActive(networkId)) {
      JsonDocument doc;
      doc["error"] = "Cannot delete active network";
      doc["status"] = "error";
      doc["id"] = networkId;
      sendJsonAsync(request, 409, doc);
      return;
    }

    // Clear network credentials.
    // findNetworkById only returns chained slots, so networkPtr is always ChainedWifiParameterGroup*.
    ChainedWifiParameterGroup* network = (ChainedWifiParameterGroup*)networkPtr;
    String deletedSsid = String(network->wifiSsid);
    network->wifiSsid[0] = '\0';
    network->wifiPassword[0] = '\0';

    // If the deleted SSID matches iwcWifi0 (the active slot), clear that too.
    WifiParameterGroup* primaryWifi = _iotWebConf->getWifiParameterGroup();
    if (deletedSsid.length() > 0 && deletedSsid == String(primaryWifi->_wifiSsid))
    {
      primaryWifi->_wifiSsid[0] = '\0';
      primaryWifi->_wifiPassword[0] = '\0';
    }

    // Save to EEPROM
    _iotWebConf->saveConfig();

    // Return success
    JsonDocument resDoc;
    resDoc["status"] = "deleted";
    resDoc["id"] = networkId;
    sendJsonAsync(request, 200, resDoc);
  });

  // 7. POST /api/networks/{id}/connect - Connect to saved network
  // ⚠️ IMPORTANT: Must use 5-arg form (with dummy body handler) for regex POST routes.
  // ESPAsyncWebServer does not call the onRequest handler for regex POST routes
  // that lack an onBody callback — even for bodyless requests — resulting in 501.
  // The dummy onBody handler ensures the handler is properly selected and onRequest
  // is always called after body processing completes (or immediately if no body).
  _asyncServer->on("^\\/api\\/networks\\/([^\\/]+)\\/connect$", HTTP_POST,
    [this](AsyncWebServerRequest* request) {
      Serial.printf("[DEBUG] POST /api/networks/*/connect - onRequest FIRED url=%s\n", request->url().c_str());
      // Extract network ID from URL
      String uri = request->url();
      String networkId = extractNetworkId(uri, "/api/networks/");

      if (networkId.length() == 0) {
        JsonDocument doc;
        doc["error"] = "Network ID missing in URL";
        sendJsonAsync(request, 400, doc);
        return;
      }

      // Find the network
      bool isSingle = false;
      void* networkPtr = findNetworkById(networkId, isSingle);
      if (networkPtr == nullptr) {
        JsonDocument doc;
        doc["error"] = "Network not found";
        sendJsonAsync(request, 404, doc);
        return;
      }

      // Check if network has credentials and extract them.
      // findNetworkById only returns chained slots.
      ChainedWifiParameterGroup* chainedNet = (ChainedWifiParameterGroup*)networkPtr;
      const char* networkSsid     = chainedNet->wifiSsid;
      const char* networkPassword = chainedNet->wifiPassword;

      if (strlen(networkSsid) == 0) {
        JsonDocument doc;
        doc["error"] = "Network not configured (SSID empty)";
        sendJsonAsync(request, 400, doc);
        return;
      }

      // Copy credentials to primary WiFi parameters
      WifiParameterGroup* primaryWifi = _iotWebConf->getWifiParameterGroup();
      strncpy(primaryWifi->_wifiSsid, networkSsid, IOTWEBCONF_WORD_LEN);
      primaryWifi->_wifiSsid[IOTWEBCONF_WORD_LEN - 1] = '\0';
      
      strncpy(primaryWifi->_wifiPassword, networkPassword, IOTWEBCONF_PASSWORD_LEN);
      primaryWifi->_wifiPassword[IOTWEBCONF_PASSWORD_LEN - 1] = '\0';

      _iotWebConf->saveConfig();

      // ⚠️ CRITICAL: Send response BEFORE triggering WiFi reconnection
      JsonDocument doc;
      doc["status"] = "connecting";
      doc["id"] = networkId;
      doc["ssid"] = networkSsid;
      sendJsonAsync(request, 200, doc);
      
      // Allow response transmission before WiFi change
      // 500ms gives the async TCP stack enough time to flush the response
      // before WiFi is disconnected by forceWifiConnection()
      delay(500);
      
      // Now trigger connection (this changes WiFi state and may disconnect)
      _iotWebConf->forceWifiConnection();
    },
    nullptr, // no upload handler
    [](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total) {
      // Dummy body handler — required for ESPAsyncWebServer to correctly route
      // POST requests to regex-matched handlers. Does nothing with body data.
      Serial.printf("[DEBUG] POST /api/networks/*/connect - onBody FIRED len=%u index=%u total=%u\n", len, index, total);
      (void)data; (void)len; (void)index; (void)total;
    }
  );

  // 8. POST /api/reconnect - Force reconnection
  _asyncServer->on("/api/reconnect", HTTP_POST, [this](AsyncWebServerRequest* request) {
    // ⚠️ CRITICAL: Send response BEFORE triggering WiFi reconnection
    JsonDocument doc;
    doc["status"] = "reconnecting";
    sendJsonAsync(request, 200, doc);
    
    // Give the HTTP response time to be transmitted before network changes
    delay(100);
    
    // Now trigger reconnection (this changes WiFi state and may disconnect)
    _iotWebConf->forceWifiConnection();
  });

  // 9. GET /api/config - Get configuration
  _asyncServer->on("/api/config", HTTP_GET, [this](AsyncWebServerRequest* request) {
    JsonDocument doc;
    JsonObject root = doc.to<JsonObject>();
    
    ParameterGroup* group = _iotWebConf->getRootParameterGroup();
    group->saveToJson(root);
    
    sendJsonAsync(request, 200, doc);
  });

  // 10. POST /api/config - Update configuration
  _asyncServer->on("/api/config", HTTP_POST, [this](AsyncWebServerRequest* request) {}, nullptr,
    [this](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total) {
      if (index == 0) {
        String body = String((char*)data);
        JsonDocument doc;
        DeserializationError error = deserializeJson(doc, body);

        if (error) {
          JsonDocument errDoc;
          errDoc["error"] = "Invalid JSON";
          sendJsonAsync(request, 400, errDoc);
          return;
        }

        JsonObject root = doc.as<JsonObject>();
        _iotWebConf->getRootParameterGroup()->loadFromJson(root);
        
        _iotWebConf->saveConfig();
        
        if (_configSavedCallback) {
          _configSavedCallback();
        }

        JsonDocument resDoc;
        resDoc["status"] = "Saved";
        sendJsonAsync(request, 200, resDoc);
      }
    });

  // ═══════════════════════════════════════════════════════
  // LEGACY REMOVED (Phase 3 Hard Fork) - ASYNC:
  // - POST /api/wifi (redundant with POST /api/networks)
  // - POST /api/networks/{id} (manual slot update - use auto-slot instead)
  // ═══════════════════════════════════════════════════════
}
#endif // IOTWEBCONF_API_HAS_ASYNC_SUPPORT

} // namespace iotwebconf

#endif // IOTWEBCONF_ENABLE_JSON
