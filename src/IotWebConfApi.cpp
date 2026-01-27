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
  // Register existing API handlers
  _server->on("/api/status", HTTP_GET, [this]() { this->handleApiStatus(); });
  _server->on("/api/networks", HTTP_GET, [this]() { this->handleApiNetworks(); });
  _server->on("/api/wifi", HTTP_POST, [this]() { this->handleApiWifiConnect(); });
  _server->on("/api/config", HTTP_GET, [this]() { this->handleApiGetConfig(); });
  _server->on("/api/config", HTTP_POST, [this]() { this->handleApiSetConfig(); });
  _server->on("/api/scan", HTTP_POST, [this]() { this->handleApiScan(); });

  // ═══════════════════════════════════════════════════════
  // PHASE 2 NETWORK MANAGEMENT API - NEW ROUTES
  // ═══════════════════════════════════════════════════════

  // GET /api/networks/configured - List all configured networks
  _server->on("/api/networks/configured", HTTP_GET, [this]() {
    this->handleApiGetConfiguredNetworks();
  });

  // POST /api/networks/{id} - Update network credentials
  // Note: WebServer doesn't easily support path parameters in the route definition,
  // so we use a wildcard pattern and parse the ID in the handler
  _server->on(UriBraces("/api/networks/{}"), HTTP_POST, [this]() {
    this->handleApiUpdateNetwork();
  });

  // POST /api/networks/{id}/connect - Connect to specific network
  _server->on(UriBraces("/api/networks/{}/connect"), HTTP_POST, [this]() {
    this->handleApiConnectToNetwork();
  });

  // POST /api/reconnect - Force reconnection with current settings
  _server->on("/api/reconnect", HTTP_POST, [this]() {
    this->handleApiReconnect();
  });
}

void IotWebConfApi::loop()
{
  // -- Handle async scan results
  if (_scanRequested)
  {
    int n = WiFi.scanComplete();
    if (n >= 0)
    {
      _scanRequested = false;
      // We don't do anything here, the results are cached by WiFi class until deleted
    }
    else if (n == -2)
    {
      // Scan failed
      _scanRequested = false;
    }
  }
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
  int n = WiFi.scanComplete();
  
  if (n == -2) 
  {
    // Scan not triggered
    JsonDocument doc;
    doc["error"] = "Scan not triggered or in progress";
    sendJson(400, doc);
    return;
  }
  else if (n == -1)
  {
    // Scan in progress
    JsonDocument doc;
    doc["status"] = "scanning";
    sendJson(202, doc); // Accepted/Processing
    return;
  }

  // Use dynamic document for lists, size depends on networks found
  // Allocating enough for ~20 networks. 
  JsonDocument doc;
  JsonArray networks = doc["networks"].to<JsonArray>();

  for (int i = 0; i < n; ++i)
  {
    JsonObject net = networks.add<JsonObject>();
    net["ssid"] = WiFi.SSID(i);
    net["rssi"] = WiFi.RSSI(i);
    net["encryption"] = WiFi.encryptionType(i);
    // net["bssid"] = WiFi.BSSIDstr(i); // Optional
    // net["channel"] = WiFi.channel(i); // Optional
  }
  
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

void IotWebConfApi::handleApiWifiConnect()
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

  const char* ssid = doc["ssid"];
  const char* password = doc["password"];

  if (!ssid) 
  {
    JsonDocument errDoc;
    errDoc["error"] = "SSID missing";
    sendJson(400, errDoc);
    return;
  }

  // Update IotWebConf parameters
  // Note: This modifies the RAM values. To persist, we might need to save.
  // Accessing WifiParameterGroup to update values
  ParameterGroup* wifiGroup = _iotWebConf->getWifiParameterGroup();
  
  // We need to find the SSID and Password parameters. 
  Parameter* ssidParam = _iotWebConf->getWifiSsidParameter();
  Parameter* pwdParam = _iotWebConf->getWifiPasswordParameter();

  strncpy(ssidParam->valueBuffer, ssid, ssidParam->getLength());
  if (password) {
      strncpy(pwdParam->valueBuffer, password, pwdParam->getLength());
  } else {
      // Open network? Or keep existing? Usually means open or empty.
      pwdParam->valueBuffer[0] = '\0';
  }

  // Save the config to EEPROM
  _iotWebConf->saveConfig();
  
  // Trigger connection
  _iotWebConf->forceWifiConnection();

  JsonDocument resDoc;
  resDoc["status"] = "Connecting";
  sendJson(200, resDoc);
}

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

WifiParameterGroup* IotWebConfApi::findNetworkById(const String& id)
{
  // 1. Check single WiFi mode (always exists - "iwcWifi0")
  WifiParameterGroup* singleWifi = _iotWebConf->getWifiParameterGroup();
  if (id == singleWifi->getId())
  {
    return singleWifi;
  }

  // 2. Check multiple WiFi mode (if configured)
  if (_multipleWifi != nullptr)
  {
    ChainedWifiParameterGroup* current = _multipleWifi->_firstSet;
    while (current != nullptr)
    {
      if (id == current->getId())
      {
        return current; // ChainedWifiParameterGroup inherits from WifiParameterGroup
      }
      current = (ChainedWifiParameterGroup*)current->getNext();
    }
  }

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
    ChainedWifiParameterGroup* current = _multipleWifi->_firstSet;
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

void IotWebConfApi::handleApiUpdateNetwork()
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
  WifiParameterGroup* network = findNetworkById(networkId);
  if (network == nullptr)
  {
    JsonDocument doc;
    doc["error"] = "Network not found";
    sendJson(404, doc);
    return;
  }

  // 3. Parse request body
  if (!_server->hasArg("plain"))
  {
    JsonDocument doc;
    doc["error"] = "Request body missing";
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
    sendJson(400, doc);
    return;
  }

  const char* ssid = reqDoc["ssid"];
  const char* password = reqDoc["password"];

  if (!ssid)
  {
    JsonDocument doc;
    doc["error"] = "SSID field required";
    sendJson(400, doc);
    return;
  }

  // 4. Update the network parameters
  strncpy(network->_wifiSsid, ssid, IOTWEBCONF_WORD_LEN);
  network->_wifiSsid[IOTWEBCONF_WORD_LEN - 1] = '\0'; // Ensure null termination

  if (password)
  {
    strncpy(network->_wifiPassword, password, IOTWEBCONF_PASSWORD_LEN);
    network->_wifiPassword[IOTWEBCONF_PASSWORD_LEN - 1] = '\0';
  }
  else
  {
    // No password provided - clear it (open network)
    network->_wifiPassword[0] = '\0';
  }

  // 5. Save to EEPROM
  // ⚠️ WARNING: This writes to EEPROM, which has limited write cycles (~100,000)
  _iotWebConf->saveConfig();

  // 6. Return success
  JsonDocument resDoc;
  resDoc["status"] = "updated";
  resDoc["id"] = networkId;
  sendJson(200, resDoc);
}

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
  WifiParameterGroup* network = findNetworkById(networkId);
  if (network == nullptr)
  {
    JsonDocument doc;
    doc["error"] = "Network not found";
    sendJson(404, doc);
    return;
  }

  // 3. Check if network has credentials configured
  if (strlen(network->_wifiSsid) == 0)
  {
    JsonDocument doc;
    doc["error"] = "Network not configured (SSID empty)";
    sendJson(400, doc);
    return;
  }

  // 4. Copy credentials to primary WiFi parameters
  //    (IotWebConf uses the primary WiFi slot for connections)
  WifiParameterGroup* primaryWifi = _iotWebConf->getWifiParameterGroup();
  strncpy(primaryWifi->_wifiSsid, network->_wifiSsid, IOTWEBCONF_WORD_LEN);
  primaryWifi->_wifiSsid[IOTWEBCONF_WORD_LEN - 1] = '\0';
  
  strncpy(primaryWifi->_wifiPassword, network->_wifiPassword, IOTWEBCONF_PASSWORD_LEN);
  primaryWifi->_wifiPassword[IOTWEBCONF_PASSWORD_LEN - 1] = '\0';

  // 5. Save config (so it persists across reboots)
  // ⚠️ WARNING: This writes to EEPROM
  _iotWebConf->saveConfig();

  // 6. Trigger connection attempt
  _iotWebConf->forceWifiConnection();

  // 7. Return success (connection happens asynchronously)
  JsonDocument doc;
  doc["status"] = "connecting";
  doc["id"] = networkId;
  doc["ssid"] = network->_wifiSsid;
  sendJson(200, doc);
}

void IotWebConfApi::handleApiReconnect()
{
  // Force WiFi reconnection with current settings
  // This does NOT modify config or write to EEPROM
  _iotWebConf->forceWifiConnection();

  JsonDocument doc;
  doc["status"] = "reconnecting";
  sendJson(200, doc);
}

#ifdef IOTWEBCONF_API_HAS_ASYNC_SUPPORT
// ═══════════════════════════════════════════════════════
// ASYNCWEBSERVER ROUTE REGISTRATION
// ═══════════════════════════════════════════════════════

void IotWebConfApi::registerRoutesAsync()
{
  // Register existing API handlers (original 6 endpoints)
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

  _asyncServer->on("/api/networks", HTTP_GET, [this](AsyncWebServerRequest* request) {
    int n = WiFi.scanComplete();
    
    if (n == -2) {
      JsonDocument doc;
      doc["error"] = "Scan not triggered or in progress";
      sendJsonAsync(request, 400, doc);
      return;
    }
    else if (n == -1) {
      JsonDocument doc;
      doc["status"] = "scanning";
      sendJsonAsync(request, 202, doc);
      return;
    }

    JsonDocument doc;
    JsonArray networks = doc["networks"].to<JsonArray>();

    for (int i = 0; i < n; ++i) {
      JsonObject net = networks.add<JsonObject>();
      net["ssid"] = WiFi.SSID(i);
      net["rssi"] = WiFi.RSSI(i);
      net["encryption"] = WiFi.encryptionType(i);
    }
    
    sendJsonAsync(request, 200, doc);
  });

  _asyncServer->on("/api/scan", HTTP_POST, [this](AsyncWebServerRequest* request) {
    WiFi.scanNetworks(true);
    _scanRequested = true;
    _lastScanTime = millis();
    
    JsonDocument doc;
    doc["status"] = "Scan started";
    sendJsonAsync(request, 202, doc);
  });

  _asyncServer->on("/api/wifi", HTTP_POST, [this](AsyncWebServerRequest* request) {}, nullptr,
    [this](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total) {
      // Body handler
      if (index == 0) {
        // First chunk - parse JSON
        String body = String((char*)data);
        JsonDocument doc;
        DeserializationError error = deserializeJson(doc, body);

        if (error) {
          JsonDocument errDoc;
          errDoc["error"] = "Invalid JSON";
          sendJsonAsync(request, 400, errDoc);
          return;
        }

        const char* ssid = doc["ssid"];
        const char* password = doc["password"];

        if (!ssid) {
          JsonDocument errDoc;
          errDoc["error"] = "SSID missing";
          sendJsonAsync(request, 400, errDoc);
          return;
        }

        Parameter* ssidParam = _iotWebConf->getWifiSsidParameter();
        Parameter* pwdParam = _iotWebConf->getWifiPasswordParameter();

        strncpy(ssidParam->valueBuffer, ssid, ssidParam->getLength());
        if (password) {
          strncpy(pwdParam->valueBuffer, password, pwdParam->getLength());
        } else {
          pwdParam->valueBuffer[0] = '\0';
        }

        _iotWebConf->saveConfig();
        _iotWebConf->forceWifiConnection();

        JsonDocument resDoc;
        resDoc["status"] = "Connecting";
        sendJsonAsync(request, 200, resDoc);
      }
    });

  _asyncServer->on("/api/config", HTTP_GET, [this](AsyncWebServerRequest* request) {
    JsonDocument doc;
    JsonObject root = doc.to<JsonObject>();
    
    ParameterGroup* group = _iotWebConf->getRootParameterGroup();
    group->saveToJson(root);
    
    sendJsonAsync(request, 200, doc);
  });

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
  // PHASE 2 NETWORK MANAGEMENT API - ASYNC ROUTES
  // ═══════════════════════════════════════════════════════

  // GET /api/networks/configured - List all configured networks
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
      ChainedWifiParameterGroup* current = _multipleWifi->_firstSet;
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

  // POST /api/networks/{id} - Update network credentials
  _asyncServer->on("^\\/api\\/networks\\/([^\\/]+)$", HTTP_POST, [this](AsyncWebServerRequest* request) {}, nullptr,
    [this](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total) {
      if (index == 0) {
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
        WifiParameterGroup* network = findNetworkById(networkId);
        if (network == nullptr) {
          JsonDocument doc;
          doc["error"] = "Network not found";
          sendJsonAsync(request, 404, doc);
          return;
        }

        // Parse request body
        String body = String((char*)data);
        JsonDocument reqDoc;
        DeserializationError error = deserializeJson(reqDoc, body);

        if (error) {
          JsonDocument doc;
          doc["error"] = "Invalid JSON";
          sendJsonAsync(request, 400, doc);
          return;
        }

        const char* ssid = reqDoc["ssid"];
        const char* password = reqDoc["password"];

        if (!ssid) {
          JsonDocument doc;
          doc["error"] = "SSID field required";
          sendJsonAsync(request, 400, doc);
          return;
        }

        // Update the network parameters
        strncpy(network->_wifiSsid, ssid, IOTWEBCONF_WORD_LEN);
        network->_wifiSsid[IOTWEBCONF_WORD_LEN - 1] = '\0';

        if (password) {
          strncpy(network->_wifiPassword, password, IOTWEBCONF_PASSWORD_LEN);
          network->_wifiPassword[IOTWEBCONF_PASSWORD_LEN - 1] = '\0';
        } else {
          network->_wifiPassword[0] = '\0';
        }

        _iotWebConf->saveConfig();

        JsonDocument resDoc;
        resDoc["status"] = "updated";
        resDoc["id"] = networkId;
        sendJsonAsync(request, 200, resDoc);
      }
    });

  // POST /api/networks/{id}/connect - Connect to specific network
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
    WifiParameterGroup* network = findNetworkById(networkId);
    if (network == nullptr) {
      JsonDocument doc;
      doc["error"] = "Network not found";
      sendJsonAsync(request, 404, doc);
      return;
    }

    // Check if network has credentials configured
    if (strlen(network->_wifiSsid) == 0) {
      JsonDocument doc;
      doc["error"] = "Network not configured (SSID empty)";
      sendJsonAsync(request, 400, doc);
      return;
    }

    // Copy credentials to primary WiFi parameters
    WifiParameterGroup* primaryWifi = _iotWebConf->getWifiParameterGroup();
    strncpy(primaryWifi->_wifiSsid, network->_wifiSsid, IOTWEBCONF_WORD_LEN);
    primaryWifi->_wifiSsid[IOTWEBCONF_WORD_LEN - 1] = '\0';
    
    strncpy(primaryWifi->_wifiPassword, network->_wifiPassword, IOTWEBCONF_PASSWORD_LEN);
    primaryWifi->_wifiPassword[IOTWEBCONF_PASSWORD_LEN - 1] = '\0';

    _iotWebConf->saveConfig();
    _iotWebConf->forceWifiConnection();

    JsonDocument doc;
    doc["status"] = "connecting";
    doc["id"] = networkId;
    doc["ssid"] = network->_wifiSsid;
    sendJsonAsync(request, 200, doc);
  });

  // POST /api/reconnect - Force reconnection with current settings
  _asyncServer->on("/api/reconnect", HTTP_POST, [this](AsyncWebServerRequest* request) {
    _iotWebConf->forceWifiConnection();

    JsonDocument doc;
    doc["status"] = "reconnecting";
    sendJsonAsync(request, 200, doc);
  });
}
#endif // IOTWEBCONF_API_HAS_ASYNC_SUPPORT

} // namespace iotwebconf

#endif // IOTWEBCONF_ENABLE_JSON
