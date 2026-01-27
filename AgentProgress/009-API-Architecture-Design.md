# Phase 2 Architecture Design: Network Management API

**Date:** 2026-01-27  
**Document:** 009  
**Architect:** @architect  
**Phase:** 2 - JSON API Enhancement  
**Status:** 🏗️ ARCHITECTURE DESIGN COMPLETE

---

## Executive Summary

This document provides the complete architectural design for extending `IotWebConfApi` to support comprehensive network management via JSON endpoints. The design supports **both single and multiple WiFi modes** seamlessly, maintains backward compatibility, and works with both synchronous and asynchronous web servers.

**Key Design Principle:** *Abstraction over detection* - Use a uniform interface that works regardless of WiFi mode, rather than complex mode detection logic.

---

## Table of Contents

1. [Architecture Overview](#architecture-overview)
2. [Design Decisions](#design-decisions)
3. [Class Structure](#class-structure)
4. [Network Identification](#network-identification)
5. [Mode Abstraction Strategy](#mode-abstraction-strategy)
6. [API Endpoint Design](#api-endpoint-design)
7. [Implementation Details](#implementation-details)
8. [Storage Management](#storage-management)
9. [URL Routing](#url-routing)
10. [Risk Analysis](#risk-analysis)
11. [Implementation Roadmap](#implementation-roadmap)

---

## Architecture Overview

### High-Level Component Diagram

```
┌─────────────────────────────────────────────────────────────┐
│                       User Application                       │
│  (Creates IotWebConf, IotWebConfApi, MultipleWifiAddition)  │
└────────────────┬────────────────────────────────────────────┘
                 │
                 ├──────────────────┐
                 │                  │
                 ▼                  ▼
    ┌─────────────────────┐  ┌──────────────────────────┐
    │   IotWebConfApi     │  │  MultipleWifiAddition    │
    │                     │  │  (Optional - user adds)  │
    │  NEW METHODS:       │  └──────────┬───────────────┘
    │  - listNetworks()   │             │
    │  - addNetwork()     │             │ references
    │  - deleteNetwork()  │             │
    │  - connectNetwork() │             ▼
    │  - reconnect()      │  ┌──────────────────────────┐
    │                     │  │  ChainedWifiParameter    │
    │  NEW ROUTES:        │  │  Group[] (3+ networks)   │
    │  - /api/networks/   │  └──────────────────────────┘
    │    configured       │
    │  - /api/networks    │
    │    (POST/DELETE)    │
    │  - /api/reconnect   │
    └──────┬──────────────┘
           │
           │ uses
           │
           ▼
    ┌─────────────────────────────────────────┐
    │         IotWebConf Core                  │
    │                                          │
    │  - WifiParameterGroup _wifiParameters   │
    │  - ParameterGroup _allParameters        │
    │  - ParameterGroup _systemParameters     │
    │  - saveConfig()                          │
    │  - forceWifiConnection()                 │
    └──────────────────────────────────────────┘
```

---

## Design Decisions

### Decision 1: Extend IotWebConfApi (Not Separate Class)

**CHOSEN: Extend existing IotWebConfApi class**

**Rationale:**
- ✅ All API functionality in one place
- ✅ Consistent patterns with existing endpoints
- ✅ Simpler for users (single class to instantiate)
- ✅ Easy to maintain
- ❌ Class becomes larger (acceptable trade-off)

**Alternative Considered:** Create `NetworkManagementApi` helper class
- More separation of concerns
- BUT: Fragmented API, more classes for users to manage
- BUT: Duplicate patterns and access to IotWebConf internals

---

### Decision 2: Network Identification via Parameter Group ID

**CHOSEN: Use ParameterGroup ID (e.g., "iwcWifi0", "iwcWifi1")**

**Rationale:**
- ✅ Unique identifier exists already
- ✅ Doesn't change when SSID changes
- ✅ Works for empty/unconfigured networks
- ✅ Same ID used in EEPROM storage
- ✅ Consistent with IotWebConf naming

**Alternatives Considered:**
1. **Array index (0, 1, 2)** - Fragile, changes when networks deleted
2. **SSID** - Not unique, can change, fails for empty networks

**ID Format:**
- Single WiFi mode: `"iwcWifi0"` (hardcoded in IotWebConf.h line 664)
- Multiple WiFi mode: `"wifi1"`, `"wifi2"`, `"wifi3"` (user-defined in ChainedWifiParameterGroup constructor)

---

### Decision 3: Optional MultipleWifiAddition Reference

**CHOSEN: Optional pointer passed via new setter method**

**Architecture:**
```cpp
class IotWebConfApi {
private:
  IotWebConf* _iotWebConf;
  MultipleWifiAddition* _multipleWifiAddition = nullptr;  // NEW

public:
  // NEW: Users with multiple WiFi set this
  void setMultipleWifiAddition(MultipleWifiAddition* addition) {
    _multipleWifiAddition = addition;
  }
};
```

**Usage Pattern:**
```cpp
// User code
MultipleWifiAddition multiWifi(&iotWebConf, networks, 3);
IotWebConfApi api(&iotWebConf, &server);
api.setMultipleWifiAddition(&multiWifi);  // NEW - optional
```

**Rationale:**
- ✅ Explicit opt-in for multiple WiFi features
- ✅ No automatic detection complexity
- ✅ Works even if MultipleWifiAddition not compiled in
- ✅ User controls when multi-network API is active
- ✅ Simple to document

**Alternative Considered:** Auto-detect MultipleWifiAddition
- Would require scanning ParameterGroup tree
- Complex logic to identify ChainedWifiParameterGroup
- Tight coupling to internal structures
- Brittle to changes

---

### Decision 4: Storage Management - Explicit Limits

**CHOSEN: Read-only access to existing networks, no dynamic add/delete in Phase 2**

**Scope for Phase 2:**
- ✅ **LIST** all configured networks (GET /api/networks/configured)
- ✅ **CONNECT** to a specific network (POST /api/networks/{id}/connect)
- ✅ **UPDATE** existing network credentials (POST /api/networks/{id})
- ✅ **RECONNECT** with current settings (POST /api/reconnect)
- ❌ **ADD** new network slots (future enhancement)
- ❌ **DELETE** network slots (future enhancement)

**Rationale:**
- ChainedWifiParameterGroup[] is a **static array** (line 66-70 of Example 15)
- Cannot safely add/remove entries at runtime without memory management
- User defines array size at compile time
- Phase 2 focuses on **managing existing slots**
- Dynamic add/delete is Phase 3 (requires memory management strategy)

**Update Strategy:**
```cpp
// Users can UPDATE credentials in existing slots
POST /api/networks/iwcWifi0
{
  "ssid": "NewSSID",
  "password": "NewPassword"
}
```

This modifies the existing ParameterGroup, preserving the slot.

---

### Decision 5: URL Parameter Parsing Strategy

**CHOSEN: Server-agnostic string parsing helper**

**Implementation:**
```cpp
private:
  // Helper: Extract network ID from URI path
  // Works with both "/api/networks/iwcWifi0/connect" and "/api/networks/iwcWifi0"
  String extractNetworkId(const String& uri, const String& prefix) {
    if (!uri.startsWith(prefix)) return "";
    
    int idStart = prefix.length();
    int idEnd = uri.indexOf('/', idStart);
    if (idEnd == -1) idEnd = uri.length();
    
    return uri.substring(idStart, idEnd);
  }
```

**For Synchronous WebServer:**
```cpp
void handleApiConnectNetwork() {
  String uri = _server->uri();  // e.g., "/api/networks/iwcWifi0/connect"
  String networkId = extractNetworkId(uri, "/api/networks/");
  // ... use networkId
}
```

**For AsyncWebServer:**
```cpp
// In registerRoutesAsync():
_asyncServer->on("^\\/api\\/networks\\/([^/]+)\\/connect$", HTTP_POST, 
  [this](AsyncWebServerRequest* request) {
    String networkId = request->pathArg(0);  // Captures first group
    handleApiConnectNetworkAsync(request, networkId);
  });
```

**Rationale:**
- ✅ Clean separation of concerns
- ✅ Each server uses its native parsing
- ✅ Helper method for sync server simplicity
- ✅ AsyncWebServer regex patterns are powerful

---

## Class Structure

### IotWebConfApi - Extended Interface

```cpp
class IotWebConfApi
{
public:
  IotWebConfApi(IotWebConf* iotWebConf, WebServer* server);

  // -- Existing methods (unchanged)
  void setConfigSavedCallback(std::function<void()> func);
  void registerRoutes();
  void loop();
  void handleApiStatus();
  void handleApiNetworks();
  void handleApiWifiConnect();
  void handleApiGetConfig();
  void handleApiSetConfig();
  void handleApiScan();

  // ═══════════════════════════════════════════════════════
  // NEW PHASE 2 METHODS
  // ═══════════════════════════════════════════════════════

  /**
   * Set the MultipleWifiAddition instance if using multiple WiFi.
   * This enables the API to manage multiple network configurations.
   * Optional - only needed if MultipleWifiAddition is used.
   */
  void setMultipleWifiAddition(MultipleWifiAddition* addition);

  /**
   * List all configured network credentials.
   * GET /api/networks/configured
   * 
   * Response format:
   * {
   *   "networks": [
   *     {"id": "iwcWifi0", "ssid": "HomeNet", "isActive": true},
   *     {"id": "wifi1", "ssid": "OfficeNet", "isActive": false}
   *   ]
   * }
   */
  void handleApiNetworksConfigured();

  /**
   * Update credentials for a specific network.
   * POST /api/networks/{id}
   * 
   * Request body:
   * {"ssid": "NewSSID", "password": "NewPassword"}
   * 
   * Updates the specified network slot and saves config.
   */
  void handleApiUpdateNetwork();

  /**
   * Connect to a specific saved network.
   * POST /api/networks/{id}/connect
   * 
   * Changes the active WiFi credentials to the specified network
   * and triggers connection attempt.
   */
  void handleApiConnectNetwork();

  /**
   * Force WiFi reconnection with current settings.
   * POST /api/reconnect
   * 
   * Useful after network changes or connection issues.
   */
  void handleApiReconnect();

private:
  IotWebConf* _iotWebConf;
  WebServer* _server;
  std::function<void()> _configSavedCallback = nullptr;
  
  // ═══════════════════════════════════════════════════════
  // NEW PHASE 2 MEMBERS
  // ═══════════════════════════════════════════════════════
  
  /**
   * Optional reference to MultipleWifiAddition.
   * Set via setMultipleWifiAddition() if user is using multiple WiFi.
   */
  MultipleWifiAddition* _multipleWifiAddition = nullptr;

  // -- Existing async scan handling
  unsigned long _lastScanTime = 0;
  bool _scanRequested = false;
  
  void sendJson(int code, JsonDocument& doc);

  // ═══════════════════════════════════════════════════════
  // NEW PHASE 2 HELPER METHODS
  // ═══════════════════════════════════════════════════════

  /**
   * Extract network ID from URL path.
   * Example: "/api/networks/iwcWifi0/connect" -> "iwcWifi0"
   */
  String extractNetworkId(const String& uri, const String& prefix);

  /**
   * Find a network parameter group by ID.
   * Searches both single WiFi mode and multiple WiFi mode.
   * Returns nullptr if not found.
   */
  WifiParameterGroup* findNetworkById(const String& id);

  /**
   * Get list of all network parameter groups.
   * Returns vector of {id, WifiParameterGroup*} pairs.
   */
  std::vector<std::pair<String, WifiParameterGroup*>> getAllNetworks();
};
```

---

## Network Identification

### ID Assignment Strategy

**Single WiFi Mode:**
- ID: `"iwcWifi0"` (hardcoded in `IotWebConf.h` line 664)
- Always present
- Only one network

**Multiple WiFi Mode:**
- IDs: User-defined in ChainedWifiParameterGroup constructor
- Example from `IotWebConf15MultipleWifi.ino` lines 66-70:
  ```cpp
  ChainedWifiParameterGroup networks[] = {
    ChainedWifiParameterGroup("wifi1"),  // <- ID is "wifi1"
    ChainedWifiParameterGroup("wifi2"),  // <- ID is "wifi2"
    ChainedWifiParameterGroup("wifi3")   // <- ID is "wifi3"
  };
  ```

**Best Practice Recommendation:**
Document that users should use descriptive IDs:
```cpp
// Good: Descriptive IDs
ChainedWifiParameterGroup networks[] = {
  ChainedWifiParameterGroup("home"),
  ChainedWifiParameterGroup("office"),
  ChainedWifiParameterGroup("mobile")
};

// Also Good: Numbered IDs
ChainedWifiParameterGroup networks[] = {
  ChainedWifiParameterGroup("wifi1"),
  ChainedWifiParameterGroup("wifi2"),
  ChainedWifiParameterGroup("wifi3")
};
```

### ID Lookup Logic

```cpp
WifiParameterGroup* IotWebConfApi::findNetworkById(const String& id) {
  // 1. Check single WiFi mode (always exists)
  WifiParameterGroup* singleWifi = _iotWebConf->getWifiParameterGroup();
  if (id == singleWifi->getId()) {
    return singleWifi;
  }

  // 2. Check multiple WiFi mode (if configured)
  if (_multipleWifiAddition != nullptr) {
    ChainedWifiParameterGroup* current = _multipleWifiAddition->_firstSet;
    while (current != nullptr) {
      if (id == current->getId()) {
        return current;
      }
      current = (ChainedWifiParameterGroup*)current->getNext();
    }
  }

  return nullptr;  // Not found
}
```

---

## Mode Abstraction Strategy

### Unified Network Iteration

Instead of detecting "single vs multiple" mode, we iterate **all available networks** uniformly:

```cpp
std::vector<std::pair<String, WifiParameterGroup*>> 
IotWebConfApi::getAllNetworks() {
  std::vector<std::pair<String, WifiParameterGroup*>> networks;

  // 1. Single WiFi mode - always present
  WifiParameterGroup* singleWifi = _iotWebConf->getWifiParameterGroup();
  if (strlen(singleWifi->_wifiSsid) > 0) {  // Only if configured
    networks.push_back({String(singleWifi->getId()), singleWifi});
  }

  // 2. Multiple WiFi mode - optional
  if (_multipleWifiAddition != nullptr) {
    ChainedWifiParameterGroup* current = _multipleWifiAddition->_firstSet;
    while (current != nullptr) {
      if (current->isActive()) {  // Only active networks
        networks.push_back({String(current->getId()), current});
      }
      current = (ChainedWifiParameterGroup*)current->getNext();
    }
  }

  return networks;
}
```

**Key Insight:** We don't need to "detect" mode - we just iterate what exists!

### Active Network Detection

A network is considered "active" if:
1. **Single WiFi:** SSID is non-empty
2. **Multiple WiFi:** `isActive()` returns true (user has filled in the optional group)

---

## API Endpoint Design

### Endpoint 1: GET /api/networks/configured

**Purpose:** List all stored WiFi credentials

**Response Format:**
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
      "id": "wifi1",
      "ssid": "OfficeNetwork",
      "isActive": true,
      "hasPassword": true,
      "isCurrent": false
    },
    {
      "id": "wifi2",
      "ssid": "",
      "isActive": false,
      "hasPassword": false,
      "isCurrent": false
    }
  ]
}
```

**Implementation Pseudocode:**
```cpp
void IotWebConfApi::handleApiNetworksConfigured() {
  JsonDocument doc;
  JsonArray networks = doc["networks"].to<JsonArray>();

  // Get current WiFi SSID for comparison
  String currentSsid = WiFi.SSID();

  // Iterate all networks
  for (auto& pair : getAllNetworks()) {
    String id = pair.first;
    WifiParameterGroup* group = pair.second;

    JsonObject net = networks.add<JsonObject>();
    net["id"] = id;
    net["ssid"] = group->_wifiSsid;
    net["isActive"] = (strlen(group->_wifiSsid) > 0);
    net["hasPassword"] = (strlen(group->_wifiPassword) > 0);
    net["isCurrent"] = (String(group->_wifiSsid) == currentSsid);
  }

  sendJson(200, doc);
}
```

---

### Endpoint 2: POST /api/networks/{id}

**Purpose:** Update credentials for a specific network

**Request Body:**
```json
{
  "ssid": "NewSSID",
  "password": "NewPassword"
}
```

**Response:**
```json
{
  "status": "updated",
  "id": "iwcWifi0"
}
```

**Implementation Pseudocode:**
```cpp
void IotWebConfApi::handleApiUpdateNetwork() {
  // 1. Extract network ID from URL
  String uri = _server->uri();
  String networkId = extractNetworkId(uri, "/api/networks/");

  if (networkId.length() == 0) {
    JsonDocument doc;
    doc["error"] = "Network ID missing";
    sendJson(400, doc);
    return;
  }

  // 2. Find the network
  WifiParameterGroup* network = findNetworkById(networkId);
  if (network == nullptr) {
    JsonDocument doc;
    doc["error"] = "Network not found";
    sendJson(404, doc);
    return;
  }

  // 3. Parse request body
  if (!_server->hasArg("plain")) {
    JsonDocument doc;
    doc["error"] = "Body missing";
    sendJson(400, doc);
    return;
  }

  String body = _server->arg("plain");
  JsonDocument reqDoc;
  DeserializationError error = deserializeJson(reqDoc, body);

  if (error) {
    JsonDocument doc;
    doc["error"] = "Invalid JSON";
    sendJson(400, doc);
    return;
  }

  const char* ssid = reqDoc["ssid"];
  const char* password = reqDoc["password"];

  if (!ssid) {
    JsonDocument doc;
    doc["error"] = "SSID missing";
    sendJson(400, doc);
    return;
  }

  // 4. Update the network parameters
  strncpy(network->_wifiSsid, ssid, IOTWEBCONF_WORD_LEN);
  network->_wifiSsid[IOTWEBCONF_WORD_LEN - 1] = '\0';

  if (password) {
    strncpy(network->_wifiPassword, password, IOTWEBCONF_PASSWORD_LEN);
    network->_wifiPassword[IOTWEBCONF_PASSWORD_LEN - 1] = '\0';
  } else {
    network->_wifiPassword[0] = '\0';  // Clear password
  }

  // 5. Save to EEPROM
  _iotWebConf->saveConfig();

  // 6. Return success
  JsonDocument resDoc;
  resDoc["status"] = "updated";
  resDoc["id"] = networkId;
  sendJson(200, resDoc);
}
```

---

### Endpoint 3: POST /api/networks/{id}/connect

**Purpose:** Switch to and connect to a specific network

**Response:**
```json
{
  "status": "connecting",
  "id": "wifi1",
  "ssid": "OfficeNetwork"
}
```

**Implementation Pseudocode:**
```cpp
void IotWebConfApi::handleApiConnectNetwork() {
  // 1. Extract network ID
  String uri = _server->uri();
  String networkId = extractNetworkId(uri, "/api/networks/");

  if (networkId.length() == 0) {
    JsonDocument doc;
    doc["error"] = "Network ID missing";
    sendJson(400, doc);
    return;
  }

  // 2. Find the network
  WifiParameterGroup* network = findNetworkById(networkId);
  if (network == nullptr) {
    JsonDocument doc;
    doc["error"] = "Network not found";
    sendJson(404, doc);
    return;
  }

  // 3. Check if network has credentials
  if (strlen(network->_wifiSsid) == 0) {
    JsonDocument doc;
    doc["error"] = "Network not configured";
    sendJson(400, doc);
    return;
  }

  // 4. Copy credentials to primary WiFi parameters
  WifiParameterGroup* primaryWifi = _iotWebConf->getWifiParameterGroup();
  strncpy(primaryWifi->_wifiSsid, network->_wifiSsid, IOTWEBCONF_WORD_LEN);
  strncpy(primaryWifi->_wifiPassword, network->_wifiPassword, IOTWEBCONF_PASSWORD_LEN);

  // 5. Save config (so it persists across reboots)
  _iotWebConf->saveConfig();

  // 6. Trigger connection
  _iotWebConf->forceWifiConnection();

  // 7. Return success
  JsonDocument doc;
  doc["status"] = "connecting";
  doc["id"] = networkId;
  doc["ssid"] = network->_wifiSsid;
  sendJson(200, doc);
}
```

---

### Endpoint 4: POST /api/reconnect

**Purpose:** Force reconnection with current settings

**Response:**
```json
{
  "status": "reconnecting"
}
```

**Implementation Pseudocode:**
```cpp
void IotWebConfApi::handleApiReconnect() {
  _iotWebConf->forceWifiConnection();

  JsonDocument doc;
  doc["status"] = "reconnecting";
  sendJson(200, doc);
}
```

---

## Implementation Details

### URL Routing Registration

**Synchronous WebServer:**
```cpp
void IotWebConfApi::registerRoutes() {
  // Existing routes
  _server->on("/api/status", HTTP_GET, [this]() { 
    this->handleApiStatus(); 
  });
  _server->on("/api/networks", HTTP_GET, [this]() { 
    this->handleApiNetworks(); 
  });
  _server->on("/api/wifi", HTTP_POST, [this]() { 
    this->handleApiWifiConnect(); 
  });
  _server->on("/api/config", HTTP_GET, [this]() { 
    this->handleApiGetConfig(); 
  });
  _server->on("/api/config", HTTP_POST, [this]() { 
    this->handleApiSetConfig(); 
  });
  _server->on("/api/scan", HTTP_POST, [this]() { 
    this->handleApiScan(); 
  });

  // ═══════════════════════════════════════════════════════
  // NEW PHASE 2 ROUTES
  // ═══════════════════════════════════════════════════════

  // List configured networks
  _server->on("/api/networks/configured", HTTP_GET, [this]() {
    this->handleApiNetworksConfigured();
  });

  // Update network (synchronous server limitation: can't parse path params easily)
  // Use a generic handler and parse URI inside
  _server->on(UriBraces("/api/networks/{}"), HTTP_POST, [this]() {
    this->handleApiUpdateNetwork();
  });

  // Connect to network (uses same pattern)
  _server->on(UriBraces("/api/networks/{}/connect"), HTTP_POST, [this]() {
    this->handleApiConnectNetwork();
  });

  // Reconnect
  _server->on("/api/reconnect", HTTP_POST, [this]() {
    this->handleApiReconnect();
  });
}
```

**Note:** ESP8266WebServer/WebServer supports `UriBraces()` for path parameters since v3.0.0. For older versions, we'll need manual URI parsing in the handlers.

---

### Async Server Support (Future Phase)

For AsyncWebServer, add a parallel method:

```cpp
#if defined(ESP_ASYNC_WEB_SERVER_H_) || defined(_ESPAsyncWebServer_H_)

void IotWebConfApi::registerRoutesAsync(AsyncWebServer* asyncServer) {
  // List configured networks
  asyncServer->on("/api/networks/configured", HTTP_GET, 
    [this](AsyncWebServerRequest* request) {
      this->handleApiNetworksConfiguredAsync(request);
    });

  // Update network - regex captures {id}
  asyncServer->on("^\\/api\\/networks\\/([^/]+)$", HTTP_POST,
    [this](AsyncWebServerRequest* request) {
      String networkId = request->pathArg(0);
      this->handleApiUpdateNetworkAsync(request, networkId);
    });

  // Connect to network - regex captures {id}
  asyncServer->on("^\\/api\\/networks\\/([^/]+)\\/connect$", HTTP_POST,
    [this](AsyncWebServerRequest* request) {
      String networkId = request->pathArg(0);
      this->handleApiConnectNetworkAsync(request, networkId);
    });

  // Reconnect
  asyncServer->on("/api/reconnect", HTTP_POST,
    [this](AsyncWebServerRequest* request) {
      this->handleApiReconnectAsync(request);
    });
}

#endif
```

**Async handler signatures:**
```cpp
void handleApiNetworksConfiguredAsync(AsyncWebServerRequest* request);
void handleApiUpdateNetworkAsync(AsyncWebServerRequest* request, const String& networkId);
void handleApiConnectNetworkAsync(AsyncWebServerRequest* request, const String& networkId);
void handleApiReconnectAsync(AsyncWebServerRequest* request);
```

---

## Storage Management

### When to Save Config

**Rule:** Save immediately after parameter changes that should persist.

**Operations that save:**
1. `handleApiUpdateNetwork()` - Saves after updating credentials
2. `handleApiConnectNetwork()` - Saves before connecting (so it persists)

**Operations that don't save:**
3. `handleApiNetworksConfigured()` - Read-only
4. `handleApiReconnect()` - Just reconnects, doesn't change config

### EEPROM Write Frequency Concerns

**Issue:** EEPROM has limited write cycles (~100,000 writes)

**Mitigation:**
1. **Document best practices** - Users should not call update endpoints in loops
2. **No automatic debouncing** - Users control when they update
3. **Warning in API docs** - Excessive updates can wear out EEPROM
4. **Future enhancement** - Add optional write delay/debouncing

**Example Documentation:**
```
⚠️ EEPROM Write Warning
The POST /api/networks/{id} endpoint saves to EEPROM immediately.
Avoid calling this endpoint repeatedly in quick succession.
EEPROM has a limited lifetime of ~100,000 write cycles.
```

### Memory Management

**Phase 2 Constraint:** No dynamic memory allocation for networks

- ChainedWifiParameterGroup[] is **static array**
- User defines size at compile time
- We only **read** and **update** existing slots
- Add/delete requires Phase 3 memory management strategy

---

## URL Routing

### Synchronous WebServer Approach

**Option 1: UriBraces (WebServer v3.0.0+)**
```cpp
_server->on(UriBraces("/api/networks/{}"), HTTP_POST, [this]() {
  String id = _server->pathArg(0);
  // ... use id
});
```

**Option 2: Manual Parsing (Universal)**
```cpp
_server->on("/api/networks/update", HTTP_POST, [this]() {
  this->handleApiUpdateNetwork();  // Parses URI inside
});

// Inside handler:
String IotWebConfApi::extractNetworkId(const String& uri, const String& prefix) {
  // Extract "iwcWifi0" from "/api/networks/iwcWifi0" or "/api/networks/iwcWifi0/connect"
  if (!uri.startsWith(prefix)) return "";
  
  int idStart = prefix.length();
  int idEnd = uri.indexOf('/', idStart);
  if (idEnd == -1) idEnd = uri.length();
  
  return uri.substring(idStart, idEnd);
}
```

**Recommendation:** Use manual parsing for maximum compatibility

---

### AsyncWebServer Approach

AsyncWebServer has excellent regex support:

```cpp
// Captures {id} as path parameter
asyncServer->on("^\\/api\\/networks\\/([^/]+)$", HTTP_POST,
  [this](AsyncWebServerRequest* request) {
    String networkId = request->pathArg(0);  // First capture group
    // ... use networkId
  });

// Captures {id} before "/connect"
asyncServer->on("^\\/api\\/networks\\/([^/]+)\\/connect$", HTTP_POST,
  [this](AsyncWebServerRequest* request) {
    String networkId = request->pathArg(0);
    // ... use networkId
  });
```

**Regex Explanation:**
- `^` - Start of string
- `\\/` - Literal forward slash (escaped)
- `([^/]+)` - Capture group: one or more non-slash characters
- `$` - End of string

---

## Risk Analysis

### Risk 1: EEPROM Wear-Out ⚠️

**Probability:** Medium  
**Impact:** High (device failure)  
**Mitigation:**
- Document write frequency limits
- Add warning in API documentation
- Consider write debouncing in Phase 3
- Log write operations for debugging

---

### Risk 2: Concurrent Config Access 🔄

**Scenario:** User calls API while config portal is being used

**Probability:** Low  
**Impact:** Medium (config corruption)  
**Mitigation:**
- IotWebConf already has config saving callbacks for serialization
- Document that API and web portal shouldn't be used simultaneously
- Consider adding mutex/locking in Phase 3

---

### Risk 3: WiFi Connection During API Call 📡

**Scenario:** Device disconnects while processing API request

**Probability:** Medium  
**Impact:** Low (request fails, but config saved)  
**Mitigation:**
- Save config BEFORE triggering connection
- Return success immediately, connection happens async
- Document that client should poll `/api/status` for connection state

---

### Risk 4: Invalid Network ID ❌

**Scenario:** Client requests non-existent network ID

**Probability:** High (user error)  
**Impact:** Low (400 error)  
**Mitigation:**
- Robust ID validation in `findNetworkById()`
- Return 404 with descriptive error message
- Document valid ID format

---

### Risk 5: MultipleWifiAddition Not Set 🚫

**Scenario:** User calls API but didn't set MultipleWifiAddition reference

**Probability:** Medium  
**Impact:** Low (works fine, just single WiFi)  
**Mitigation:**
- API gracefully handles null `_multipleWifiAddition`
- Only single WiFi network returned in list
- Document that `setMultipleWifiAddition()` is optional

---

### Risk 6: Async Server Body Parsing 📦

**Scenario:** AsyncWebServer parses request body differently

**Probability:** Medium  
**Impact:** Medium (async handlers need different implementation)  
**Mitigation:**
- Implement separate async handlers
- Use AsyncWebServer's body parsing callbacks
- Test thoroughly with async server

---

## Implementation Roadmap

### Phase 2A: Core Network Management (8 hours)

**Step 1: Add Class Members (30 min)**
- Add `_multipleWifiAddition` pointer
- Add `setMultipleWifiAddition()` method
- Update constructor if needed

**Step 2: Implement Helper Methods (2 hours)**
- `extractNetworkId()` - URL parsing
- `findNetworkById()` - Network lookup
- `getAllNetworks()` - Unified iteration

**Step 3: Implement Endpoint Handlers (4 hours)**
- `handleApiNetworksConfigured()` - List networks
- `handleApiUpdateNetwork()` - Update credentials
- `handleApiConnectNetwork()` - Switch network
- `handleApiReconnect()` - Reconnect

**Step 4: Register Routes (1 hour)**
- Update `registerRoutes()` method
- Test URL routing
- Handle edge cases

**Step 5: Testing (30 min)**
- Compile check
- Basic functionality test

---

### Phase 2B: AsyncWebServer Support (4 hours)

**Step 1: Add Async Header Include (15 min)**
- Conditional compilation guards
- Include AsyncWebServer.h if available

**Step 2: Implement Async Handlers (2 hours)**
- `handleApiNetworksConfiguredAsync()`
- `handleApiUpdateNetworkAsync()`
- `handleApiConnectNetworkAsync()`
- `handleApiReconnectAsync()`

**Step 3: Register Async Routes (1 hour)**
- `registerRoutesAsync()` method
- Regex pattern testing
- Path parameter extraction

**Step 4: Testing (45 min)**
- Test with AsyncWebServer
- Test body parsing
- Test regex patterns

---

### Phase 2C: Documentation & Examples (4 hours)

**Step 1: Update IotWebConfApi.h (1 hour)**
- Add inline documentation
- Document new methods
- Document usage patterns

**Step 2: Create/Update Example (2 hours)**
- Extend Example 18 or create Example 20
- Demonstrate all new endpoints
- Show both single and multiple WiFi

**Step 3: Create API Documentation (1 hour)**
- Document all endpoints
- Request/response examples
- Best practices guide

---

### Phase 2D: CAB Review & Refinement (4 hours)

**Step 1: Prepare CAB Submission**
- Architecture document (this doc)
- Code review checklist
- Risk assessment

**Step 2: Implement Feedback**
- Address CAB concerns
- Refine implementation
- Update documentation

**Step 3: Final Testing**
- End-to-end testing
- Edge case validation
- Performance check

---

## Success Criteria

- [ ] All 4 new endpoints implemented and working
- [ ] Works with single WiFi mode (no MultipleWifiAddition)
- [ ] Works with multiple WiFi mode (with MultipleWifiAddition)
- [ ] Works with synchronous WebServer
- [ ] Works with AsyncWebServer
- [ ] No breaking changes to existing API
- [ ] Backward compatible
- [ ] Comprehensive documentation
- [ ] Example demonstrates all features
- [ ] EEPROM write warnings documented
- [ ] Zero memory leaks
- [ ] Compile with no warnings

---

## Code Quality Guidelines

### Naming Conventions
- Methods: `camelCase` (e.g., `handleApiNetworksConfigured`)
- Members: `_camelCase` with underscore prefix (e.g., `_multipleWifiAddition`)
- Constants: `UPPER_SNAKE_CASE` (e.g., `IOTWEBCONF_WORD_LEN`)

### Error Handling
- Always validate input
- Return HTTP 400 for bad requests
- Return HTTP 404 for not found
- Return HTTP 200 for success
- Include descriptive error messages in JSON

### Documentation
- Document all public methods
- Include usage examples
- Document error conditions
- Document side effects (e.g., EEPROM writes)

### Memory Management
- No dynamic allocation in Phase 2
- Use stack variables where possible
- Document memory usage
- Test for leaks

---

## Alternative Approaches Considered

### Alternative 1: Separate NetworkManagementApi Class

**Structure:**
```cpp
class NetworkManagementApi {
  IotWebConf* _iotWebConf;
  MultipleWifiAddition* _multiWifi;
public:
  void registerRoutes(WebServer* server);
  void handleListNetworks();
  // ...
};
```

**Pros:**
- Cleaner separation of concerns
- Smaller class size
- Easier to test in isolation

**Cons:**
- Users manage two API classes
- Duplicate patterns (sendJson, etc.)
- More complex initialization
- Fragmented API surface

**Decision:** Rejected - IotWebConfApi extension is simpler for users

---

### Alternative 2: Automatic MultipleWifiAddition Detection

**Approach:** Scan parameter tree for ChainedWifiParameterGroup

```cpp
void IotWebConfApi::detectMultipleWifi() {
  // Scan system parameters for ChainedWifiParameterGroup
  ParameterGroup* sysParams = _iotWebConf->getSystemParameterGroup();
  // ... complex tree traversal ...
}
```

**Pros:**
- Automatic, no user configuration
- Works transparently

**Cons:**
- Complex implementation
- Tight coupling to internal structures
- Brittle to changes
- What if structure changes?
- Performance overhead

**Decision:** Rejected - Explicit setter is clearer and simpler

---

### Alternative 3: Dynamic Network Addition/Deletion

**Approach:** Allow adding/removing ChainedWifiParameterGroup at runtime

**Pros:**
- Full CRUD operations
- Maximum flexibility

**Cons:**
- Requires dynamic memory management
- ChainedWifiParameterGroup[] is static array
- Complex lifecycle management
- Risk of memory fragmentation
- Requires ParameterGroup tree manipulation

**Decision:** Deferred to Phase 3 - Too complex for Phase 2 scope

---

## Next Steps

1. **@architect** - Submit this design to CAB for review
2. **CAB** - Review and provide feedback
3. **@architect** - Address CAB feedback
4. **@dev_senior** - Begin implementation (Phase 2A)
5. **@dev_junior** - Prepare test scenarios
6. **@librarian** - Prepare documentation templates

---

## Appendices

### Appendix A: JSON Response Schemas

**GET /api/networks/configured**
```json
{
  "networks": [
    {
      "id": "string (required)",
      "ssid": "string (required, may be empty)",
      "isActive": "boolean (required)",
      "hasPassword": "boolean (required)",
      "isCurrent": "boolean (required)"
    }
  ]
}
```

**POST /api/networks/{id}**
Request:
```json
{
  "ssid": "string (required)",
  "password": "string (optional)"
}
```
Response:
```json
{
  "status": "updated",
  "id": "string"
}
```

**POST /api/networks/{id}/connect**
Response:
```json
{
  "status": "connecting",
  "id": "string",
  "ssid": "string"
}
```

**POST /api/reconnect**
Response:
```json
{
  "status": "reconnecting"
}
```

**Error Response (400/404/500)**
```json
{
  "error": "string (descriptive message)"
}
```

---

### Appendix B: Memory Impact Analysis

**Per-Request Memory Usage:**
- JsonDocument for response: ~1KB (stack)
- String parsing: ~100 bytes (stack)
- Network iteration: ~50 bytes (stack)

**Static Memory Addition:**
- `_multipleWifiAddition` pointer: 4 bytes (ESP8266) / 8 bytes (ESP32)

**Total Static Overhead:** 4-8 bytes (0.1% of typical ESP8266 RAM)

**Total Per-Request Overhead:** ~1.2 KB (acceptable for ESP devices)

---

### Appendix C: Example Usage

**User Application Code:**
```cpp
#include <IotWebConf.h>
#include <IotWebConfApi.h>
#include <IotWebConfMultipleWifi.h>

// Setup WiFi networks
ChainedWifiParameterGroup networks[] = {
  ChainedWifiParameterGroup("home"),
  ChainedWifiParameterGroup("office"),
  ChainedWifiParameterGroup("mobile")
};

MultipleWifiAddition multiWifi(&iotWebConf, networks, 3);
IotWebConfApi api(&iotWebConf, &server);

void setup() {
  // Initialize IotWebConf
  multiWifi.init();
  iotWebConf.init();

  // NEW: Enable multiple WiFi API
  api.setMultipleWifiAddition(&multiWifi);

  // Register API routes
  api.registerRoutes();

  // Custom routes
  server.on("/", handleRoot);
}

void loop() {
  iotWebConf.doLoop();
  api.loop();  // For async scan
}
```

**Client JavaScript:**
```javascript
// List all networks
fetch('/api/networks/configured')
  .then(r => r.json())
  .then(data => console.log(data.networks));

// Update network credentials
fetch('/api/networks/home', {
  method: 'POST',
  headers: {'Content-Type': 'application/json'},
  body: JSON.stringify({
    ssid: 'MyHomeWiFi',
    password: 'secret123'
  })
});

// Connect to specific network
fetch('/api/networks/office/connect', {method: 'POST'})
  .then(r => r.json())
  .then(data => console.log(data.status));

// Force reconnect
fetch('/api/reconnect', {method: 'POST'});
```

---

## Document Metadata

**Created:** 2026-01-27  
**Author:** @architect  
**Reviewed By:** (pending CAB review)  
**Status:** COMPLETE - AWAITING CAB APPROVAL  
**Version:** 1.0  
**Total Pages:** 30+ (equivalent)

---

## Signatures

**Architecture Lead:** @architect  
**Date:** 2026-01-27

**Pending Review:**
- [ ] CAB Approval
- [ ] Technical Lead Review (@dev_senior)
- [ ] Security Review
- [ ] Documentation Review (@librarian)

---

**STATUS:** 🏗️ ARCHITECTURE DESIGN COMPLETE - READY FOR CAB REVIEW

**Next Document:** `010-CAB-Review-Results.md` (after CAB review)

---

*This architecture provides a solid foundation for Phase 2 implementation. The design is pragmatic, maintainable, and follows established patterns from the codebase. Let's build something great!* 🚀
