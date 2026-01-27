/**
 * IotWebConfApi.cpp -- JSON API for IotWebConf
 */

#include "IotWebConfApi.h"
#include "IotWebConfMultipleWifi.h"

#ifdef IOTWEBCONF_ENABLE_JSON

namespace iotwebconf
{

IotWebConfApi::IotWebConfApi(IotWebConf* iotWebConf, WebServer* server)
{
  _iotWebConf = iotWebConf;
  _server = server;
#ifdef IOTWEBCONF_API_HAS_ASYNC_SUPPORT
  _isAsync = false;
#endif
}

#ifdef IOTWEBCONF_API_HAS_ASYNC_SUPPORT
IotWebConfApi::IotWebConfApi(IotWebConf* iotWebConf, AsyncWebServer* server)
{
  _iotWebConf = iotWebConf;
  _asyncServer = server;
  _server = nullptr;
  _isAsync = true;
}
#endif

void IotWebConfApi::setConfigSavedCallback(std::function<void()> func)
{
  _configSavedCallback = func;
}

void IotWebConfApi::registerRoutes()
{
  // ═══════════════════════════════════════════════════════
  // PHASE 3 JSON API - 10 ENDPOINTS TOTAL
  // ═══════════════════════════════════════════════════════
  
  // 1. GET /api/status - Device status
  _server->on("/api/status", HTTP_GET, [this]() { this->handleApiStatus(); });
  
  // 2. POST /api/scan - Start WiFi scan
  _server->on("/api/scan", HTTP_POST, [this]() { this->handleApiScan(); });
  
  // 3. GET /api/networks - Scan results
  _server->on("/api/networks", HTTP_GET, [this]() { this->handleApiNetworks(); });
  
  // 4. GET /api/networks/configured - List saved networks
  _server->on("/api/networks/configured", HTTP_GET, [this]() {
    this->handleApiGetConfiguredNetworks();
  });
  
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


void IotWebConfApi::sendJson(int code, JsonDocument& doc)
{
  String response;
  serializeJson(doc, response);
  _server->sendHeader("Access-Control-Allow-Origin", "*");
  _server->send(code, "application/json", response);
}

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
  int slotCount = 0;
  
  // 1. Check primary WiFi slot (iwcWifi0)
  WifiParameterGroup* primaryWifi = _iotWebConf->getWifiParameterGroup();
  if (slotCount < IOTWEBCONF_MAX_WIFI_NETWORKS)
  {
    if (strlen(primaryWifi->_wifiSsid) == 0)
    {
      // Primary slot is empty - available
      outIsSingle = true;
      return (void*)primaryWifi;
    }
    slotCount++;
  }
  
  // 2. Check additional network slots (wifi1, wifi2, ...)
  if (_multipleWifi != nullptr)
  {
    ChainedWifiParameterGroup* current = _multipleWifi->getFirstSet();
    while (current != nullptr && slotCount < IOTWEBCONF_MAX_WIFI_NETWORKS)
    {
      if (strlen(current->wifiSsid) == 0)
      {
        // This slot is empty - available
        outIsSingle = false;
        return (void*)current;
      }
      slotCount++;
      current = (ChainedWifiParameterGroup*)current->getNext();
    }
  }
  
  // No available slots
  outIsSingle = false;
  return nullptr;
}

int IotWebConfApi::getConfiguredNetworkCount()
{
  int count = 0;
  
  // 1. Check primary WiFi slot
  WifiParameterGroup* primaryWifi = _iotWebConf->getWifiParameterGroup();
  if (strlen(primaryWifi->_wifiSsid) > 0)
  {
    count++;
  }
  
  // 2. Check additional network slots
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
  
  // Find the network and check if its SSID matches current connection
  bool isSingle = false;
  void* networkPtr = findNetworkById(id, isSingle);
  
  if (networkPtr == nullptr)
  {
    return false; // Network not found
  }
  
  const char* networkSsid = nullptr;
  if (isSingle)
  {
    WifiParameterGroup* network = (WifiParameterGroup*)networkPtr;
    networkSsid = network->_wifiSsid;
  }
  else
  {
    ChainedWifiParameterGroup* network = (ChainedWifiParameterGroup*)networkPtr;
    networkSsid = network->wifiSsid;
  }
  
  return (String(networkSsid) == currentSsid);
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
  // 1. Check single WiFi mode (always exists - "iwcWifi0")
  WifiParameterGroup* singleWifi = _iotWebConf->getWifiParameterGroup();
  if (id == singleWifi->getId())
  {
    outIsSingle = true;
    return (void*)singleWifi;
  }

  // 2. Check multiple WiFi mode (if configured)
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
  return nullptr; // Not found
}

void IotWebConfApi::handleApiGetConfiguredNetworks()
{
  JsonDocument doc;
  JsonArray networks = doc["networks"].to<JsonArray>();

  // Get currently connected SSID for comparison
  String currentSsid = WiFi.SSID();

  // 1. Add single WiFi network (always present - "iwcWifi0")
  WifiParameterGroup* singleWifi = _iotWebConf->getWifiParameterGroup();
  JsonObject net = networks.add<JsonObject>();
  net["id"] = singleWifi->getId();
  net["ssid"] = singleWifi->_wifiSsid;
  net["isActive"] = (strlen(singleWifi->_wifiSsid) > 0);
  net["hasPassword"] = (strlen(singleWifi->_wifiPassword) > 0);
  net["isCurrent"] = (String(singleWifi->_wifiSsid) == currentSsid);

  // 2. Add multiple WiFi networks (if MultipleWifiAddition is set)
  if (_multipleWifi != nullptr)
  {
    ChainedWifiParameterGroup* current = _multipleWifi->getFirstSet();
    while (current != nullptr)
    {
      JsonObject multiNet = networks.add<JsonObject>();
      multiNet["id"] = current->getId();
      multiNet["ssid"] = current->wifiSsid;
      multiNet["isActive"] = current->isActive(); // ChainedWifiParameterGroup has isActive()
      multiNet["hasPassword"] = (strlen(current->wifiPassword) > 0);
      multiNet["isCurrent"] = (String(current->wifiSsid) == currentSsid);
      
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

  // 3. Check if network has credentials configured and extract them
  const char* networkSsid = nullptr;
  const char* networkPassword = nullptr;
  
  if (isSingle)
  {
    WifiParameterGroup* network = (WifiParameterGroup*)networkPtr;
    networkSsid = network->_wifiSsid;
    networkPassword = network->_wifiPassword;
  }
  else
  {
    ChainedWifiParameterGroup* network = (ChainedWifiParameterGroup*)networkPtr;
    networkSsid = network->wifiSsid;
    networkPassword = network->wifiPassword;
  }

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

  // 3. Find next available slot
  bool isSingle = false;
  void* slotPtr = findAvailableSlot(isSingle);
  
  if (slotPtr == nullptr)
  {
    JsonDocument doc;
    doc["error"] = "Maximum networks reached (10)";
    doc["status"] = "error";
    sendJson(507, doc); // 507 Insufficient Storage
    return;
  }

  // 4. Update slot with credentials
  String networkId;
  
  if (isSingle)
  {
    WifiParameterGroup* network = (WifiParameterGroup*)slotPtr;
    strncpy(network->_wifiSsid, ssid, IOTWEBCONF_WORD_LEN);
    network->_wifiSsid[IOTWEBCONF_WORD_LEN - 1] = '\0';
    
    if (password)
    {
      strncpy(network->_wifiPassword, password, IOTWEBCONF_PASSWORD_LEN);
      network->_wifiPassword[IOTWEBCONF_PASSWORD_LEN - 1] = '\0';
    }
    else
    {
      network->_wifiPassword[0] = '\0';
    }
    
    networkId = network->getId();
  }
  else
  {
    ChainedWifiParameterGroup* network = (ChainedWifiParameterGroup*)slotPtr;
    strncpy(network->wifiSsid, ssid, IOTWEBCONF_WORD_LEN);
    network->wifiSsid[IOTWEBCONF_WORD_LEN - 1] = '\0';
    
    if (password)
    {
      strncpy(network->wifiPassword, password, IOTWEBCONF_PASSWORD_LEN);
      network->wifiPassword[IOTWEBCONF_PASSWORD_LEN - 1] = '\0';
    }
    else
    {
      network->wifiPassword[0] = '\0';
    }
    
    networkId = network->getId();
  }

  // 5. Save configuration to EEPROM
  _iotWebConf->saveConfig();

  // 6. Build and send response
  int slot = getNetworkSlot(networkId);
  JsonDocument resDoc;
  resDoc["status"] = "connecting";
  resDoc["id"] = networkId;
  resDoc["slot"] = slot;
  resDoc["ssid"] = ssid;
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
  if (isSingle)
  {
    WifiParameterGroup* network = (WifiParameterGroup*)networkPtr;
    network->_wifiSsid[0] = '\0';
    network->_wifiPassword[0] = '\0';
  }
  else
  {
    ChainedWifiParameterGroup* network = (ChainedWifiParameterGroup*)networkPtr;
    network->wifiSsid[0] = '\0';
    network->wifiPassword[0] = '\0';
  }

  // 5. Save to EEPROM
  _iotWebConf->saveConfig();

  // 6. Return success
  JsonDocument resDoc;
  resDoc["status"] = "deleted";
  resDoc["id"] = networkId;
  sendJson(200, resDoc);
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
    
    JsonDocument doc;
    doc["status"] = "Scan started";
    sendJsonAsync(request, 202, doc);
  });

  // 3. GET /api/networks - Scan results
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

  // 4. GET /api/networks/configured - List saved networks
  _asyncServer->on("/api/networks/configured", HTTP_GET, [this](AsyncWebServerRequest* request) {
    JsonDocument doc;
    JsonArray networks = doc["networks"].to<JsonArray>();

    String currentSsid = WiFi.SSID();

    // Add single WiFi network
    WifiParameterGroup* singleWifi = _iotWebConf->getWifiParameterGroup();
    JsonObject net = networks.add<JsonObject>();
    net["id"] = singleWifi->getId();
    net["ssid"] = singleWifi->_wifiSsid;
    net["isActive"] = (strlen(singleWifi->_wifiSsid) > 0);
    net["hasPassword"] = (strlen(singleWifi->_wifiPassword) > 0);
    net["isCurrent"] = (String(singleWifi->_wifiSsid) == currentSsid);

    // Add multiple WiFi networks if configured
    if (_multipleWifi != nullptr) {
      ChainedWifiParameterGroup* current = _multipleWifi->getFirstSet();
      while (current != nullptr) {
        JsonObject multiNet = networks.add<JsonObject>();
        multiNet["id"] = current->getId();
        multiNet["ssid"] = current->wifiSsid;
        multiNet["isActive"] = current->isActive();
        multiNet["hasPassword"] = (strlen(current->wifiPassword) > 0);
        multiNet["isCurrent"] = (String(current->wifiSsid) == currentSsid);
        
        current = (ChainedWifiParameterGroup*)current->getNext();
      }
    }

    sendJsonAsync(request, 200, doc);
  });

  // 5. POST /api/networks - Add network (auto-slot) [PHASE 3 NEW]
  _asyncServer->on("/api/networks", HTTP_POST, [this](AsyncWebServerRequest* request) {}, nullptr,
    [this](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total) {
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

        // Find next available slot
        bool isSingle = false;
        void* slotPtr = findAvailableSlot(isSingle);
        
        if (slotPtr == nullptr) {
          JsonDocument doc;
          doc["error"] = "Maximum networks reached (10)";
          doc["status"] = "error";
          sendJsonAsync(request, 507, doc);
          return;
        }

        // Update slot with credentials
        String networkId;
        
        if (isSingle) {
          WifiParameterGroup* network = (WifiParameterGroup*)slotPtr;
          strncpy(network->_wifiSsid, ssid, IOTWEBCONF_WORD_LEN);
          network->_wifiSsid[IOTWEBCONF_WORD_LEN - 1] = '\0';
          
          if (password) {
            strncpy(network->_wifiPassword, password, IOTWEBCONF_PASSWORD_LEN);
            network->_wifiPassword[IOTWEBCONF_PASSWORD_LEN - 1] = '\0';
          } else {
            network->_wifiPassword[0] = '\0';
          }
          
          networkId = network->getId();
        } else {
          ChainedWifiParameterGroup* network = (ChainedWifiParameterGroup*)slotPtr;
          strncpy(network->wifiSsid, ssid, IOTWEBCONF_WORD_LEN);
          network->wifiSsid[IOTWEBCONF_WORD_LEN - 1] = '\0';
          
          if (password) {
            strncpy(network->wifiPassword, password, IOTWEBCONF_PASSWORD_LEN);
            network->wifiPassword[IOTWEBCONF_PASSWORD_LEN - 1] = '\0';
          } else {
            network->wifiPassword[0] = '\0';
          }
          
          networkId = network->getId();
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
        sendJsonAsync(request, 200, resDoc);

        // Allow transmission before WiFi change
        delay(100);

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

    // Clear network credentials
    if (isSingle) {
      WifiParameterGroup* network = (WifiParameterGroup*)networkPtr;
      network->_wifiSsid[0] = '\0';
      network->_wifiPassword[0] = '\0';
    } else {
      ChainedWifiParameterGroup* network = (ChainedWifiParameterGroup*)networkPtr;
      network->wifiSsid[0] = '\0';
      network->wifiPassword[0] = '\0';
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
  _asyncServer->on("^\\/api\\/networks\\/([^\\/]+)\\/connect$", HTTP_POST, [this](AsyncWebServerRequest* request) {
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

    // Check if network has credentials configured and extract them
    const char* networkSsid = nullptr;
    const char* networkPassword = nullptr;
    
    if (isSingle) {
      WifiParameterGroup* network = (WifiParameterGroup*)networkPtr;
      networkSsid = network->_wifiSsid;
      networkPassword = network->_wifiPassword;
    } else {
      ChainedWifiParameterGroup* network = (ChainedWifiParameterGroup*)networkPtr;
      networkSsid = network->wifiSsid;
      networkPassword = network->wifiPassword;
    }

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
    
    // Give the HTTP response time to be transmitted before network changes
    delay(100);
    
    // Now trigger connection (this changes WiFi state and may disconnect)
    _iotWebConf->forceWifiConnection();
  });

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
