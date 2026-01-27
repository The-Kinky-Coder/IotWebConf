# Analysis: StandardWebServerWrapper Implementation

**Date:** 2026-01-27  
**Document:** 002  
**Purpose:** Understand existing wrapper to guide AsyncWebServer implementation

---

## Overview

The IotWebConf library uses a wrapper pattern to abstract web server operations. This allows different server implementations to be plugged in.

---

## Interface Definitions

### WebRequestWrapper Interface

Located in: `src/IotWebConfWebServerWrapper.h`

```cpp
class WebRequestWrapper {
public:
  virtual const String hostHeader() const;
  virtual IPAddress localIP();
  virtual uint16_t localPort();
  virtual const String uri() const;
  virtual bool authenticate(const char* username, const char* password);
  virtual void requestAuthentication();
  virtual bool hasArg(const String& name);
  virtual String arg(const String name);
  virtual void sendHeader(const String& name, const String& value, bool first = false);
  virtual void setContentLength(const size_t contentLength);
  virtual void send(int code, const char* content_type = nullptr, const String& content = String(""));
  virtual void sendContent(const String& content);
  virtual void stop();
};
```

**Purpose:** Abstracts HTTP request/response operations

---

### WebServerWrapper Interface

Located in: `src/IotWebConfWebServerWrapper.h`

```cpp
class WebServerWrapper {
public:
  virtual void handleClient();
  virtual void begin();
};
```

**Purpose:** Abstracts server lifecycle management

---

## StandardWebServerWrapper Implementation

Located in: `src/IotWebConf.h` (lines 104-145)

### StandardWebRequestWrapper

```cpp
class StandardWebRequestWrapper : public WebRequestWrapper {
public:
  StandardWebRequestWrapper(WebServer* server) { this->_server = server; };
  
  // All methods simply delegate to _server instance
  const String hostHeader() const override 
    { return this->_server->hostHeader(); };
  
  IPAddress localIP() override 
    { return this->_server->client().localIP(); };
  
  // ... etc for all interface methods
  
private:
  WebServer* _server;
  friend IotWebConf;
};
```

**Key Points:**
- Simple delegation pattern
- Wraps a WebServer pointer
- Each method calls equivalent WebServer method
- No state management needed (stateless wrapper)

---

### StandardWebServerWrapper

```cpp
class StandardWebServerWrapper : public WebServerWrapper {
public:
  StandardWebServerWrapper(WebServer* server) { this->_server = server; };
  
  void handleClient() override 
    { this->_server->handleClient(); };
  
  void begin() override 
    { this->_server->begin(); };
  
private:
  StandardWebServerWrapper() { };
  WebServer* _server;
  friend IotWebConf;
};
```

**Key Points:**
- Even simpler - only 2 methods
- Direct delegation to WebServer
- Private default constructor (prevents misuse)
- IotWebConf is friend (can access private members)

---

## How IotWebConf Uses the Wrappers

### Constructor Pattern

```cpp
IotWebConf(
  const char* thingName, 
  DNSServer* dnsServer, 
  WebServer* server,
  const char* initialApPassword, 
  const char* configVersion = "init"
) : IotWebConf(thingName, dnsServer, &this->_standardWebServerWrapper, initialApPassword, configVersion)
{
  this->_standardWebServerWrapper._server = server;
}
```

**Pattern:** 
1. User passes concrete `WebServer*`
2. Constructor wraps it in `StandardWebServerWrapper`
3. Delegates to main constructor taking `WebServerWrapper*`

---

### Usage in doLoop()

From `src/IotWebConf.cpp`:

```cpp
void IotWebConf::doLoop() {
  // ...state management...
  
  this->_webServerWrapper->handleClient(); // Called in loop
  
  // ...
}
```

---

### Usage During Initialization

From `src/IotWebConf.cpp`:

```cpp
void IotWebConf::stateChanged(NetworkState oldState, NetworkState newState) {
  // ...
  if (newState == OnLine) {
    this->_webServerWrapper->begin(); // Start server
  }
  // ...
}
```

**Important:** `begin()` is called when entering OnLine state, NOT during init()

---

## Request Wrapper Usage Pattern

When handling HTTP requests:

```cpp
void IotWebConf::handleConfig(WebRequestWrapper* webRequestWrapper) {
  // Authentication
  if (!webRequestWrapper->authenticate(IOTWEBCONF_ADMIN_USER_NAME, this->_apPassword)) {
    webRequestWrapper->requestAuthentication();
    return;
  }
  
  // Check for save request
  if (webRequestWrapper->hasArg("iotSave")) {
    // Handle save
    // ...
  }
  
  // Send response
  webRequestWrapper->sendHeader("Cache-Control", "no-cache");
  webRequestWrapper->send(200, "text/html", htmlContent);
}
```

**Pattern:**
1. Take `WebRequestWrapper*` as parameter
2. Use virtual methods for all HTTP operations
3. No direct access to underlying server

---

## Captive Portal Handling

From `src/IotWebConf.cpp`:

```cpp
bool IotWebConf::handleCaptivePortal(WebRequestWrapper* webRequestWrapper) {
  if (!this->isIp(webRequestWrapper->hostHeader())) {
    // Redirect to captive portal
    webRequestWrapper->sendHeader("Location", String("http://") + toStringIp(webRequestWrapper->localIP()), true);
    webRequestWrapper->send(302, "text/plain", "");
    webRequestWrapper->stop();
    return true;
  }
  return false;
}
```

**Key Operations:**
- Check host header
- Send redirect (302)
- Use sendHeader for Location
- Stop connection

---

## Key Insights for AsyncWebServer Implementation

### 1. Minimal Interface
Only need to implement:
- 13 methods in WebRequestWrapper
- 2 methods in WebServerWrapper

### 2. Stateless Design
Wrappers are simple pass-through objects with no state

### 3. Request Lifetime
`WebRequestWrapper` is created per-request, not stored

### 4. Authentication Pattern
- `authenticate()` checks credentials
- `requestAuthentication()` sends 401 with WWW-Authenticate header

### 5. begin() Timing
Server's `begin()` is called during state transition, not init

---

## Mapping to AsyncWebServer

| Sync Method | Async Equivalent | Notes |
|-------------|------------------|-------|
| `server.on()` | `server.on()` | Similar but different callback signature |
| `server.begin()` | `server.begin()` | Same |
| `server.handleClient()` | N/A | Not needed in async |
| `server.send()` | `request->send()` | Request-based, not server-based |
| `server.authenticate()` | `request->authenticate()` | Same signature |
| `server.requestAuthentication()` | Manual 401 | No built-in method |
| `server.hasArg()` | `request->hasArg()` | Same |
| `server.arg()` | `request->arg()` | Same |

---

## Challenges Identified

### 1. requestAuthentication() 
AsyncWebServer has no equivalent. Must manually:
```cpp
request->send(401, "text/html", "Unauthorized");
// Or build proper WWW-Authenticate response
```

### 2. handleClient() Not Needed
AsyncWebServerWrapper can have empty implementation or no-op

### 3. Route Registration Timing
- Sync: Can register routes after begin()
- Async: MUST register routes before begin()

**Solution:** Store route handlers, register in begin()

### 4. Request Object Lifetime
- Sync: Server object persists, request is implicit
- Async: Request object is passed to callback, managed by library

**Solution:** Store request pointer during callback execution

---

## Implementation Strategy

### Phase A: Create AsyncWebRequestWrapper
- Wrap `AsyncWebServerRequest*`
- Implement all 13 interface methods
- Handle requestAuthentication() specially

### Phase B: Create AsyncWebServerWrapper
- Store callback map for route handlers
- Implement begin() to register all stored routes
- Implement handleClient() as no-op

### Phase C: Add IotWebConf Constructor
- Accept `AsyncWebServer*`
- Create AsyncWebServerWrapper instance
- Initialize same as sync version

---

## Next Steps

1. Create `src/IotWebConfAsyncWebServer.h` header
2. Implement AsyncWebRequestWrapper
3. Implement AsyncWebServerWrapper with deferred route registration
4. Test with minimal example

---

**Status:** ANALYSIS COMPLETE  
**Next Document:** 003-AsyncWrapper-Implementation.md
