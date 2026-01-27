# Phase 1 Kickoff - AsyncWebServer Wrapper Implementation

**Date:** 2026-01-27  
**Phase:** 1 of 4  
**Goal:** Implement ESPAsyncWebServer wrapper to allow users to pass their own AsyncWebServer instance

---

## Objectives

1. Create `IotWebConfAsyncWebServer.h` header file
2. Implement `AsyncWebRequestWrapper` class
3. Implement `AsyncWebServerWrapper` class  
4. Add IotWebConf constructor for AsyncWebServer
5. Test basic functionality

---

## Current Understanding

### Existing Architecture

The library already has an abstraction layer:
- `WebServerWrapper` - Abstract interface for web server operations
- `WebRequestWrapper` - Abstract interface for HTTP request/response
- `StandardWebServerWrapper` - Implementation for ESP8266WebServer/WebServer

### What We Need to Build

```
AsyncWebServerWrapper : WebServerWrapper
AsyncWebRequestWrapper : WebRequestWrapper
```

These will wrap the ESPAsyncWebServer library (https://github.com/ESP32Async/ESPAsyncWebServer)

---

## Key Differences: Sync vs Async WebServer

### Synchronous WebServer
```cpp
server.on("/path", []() {
  server.send(200, "text/html", "response");
});
server.begin();
server.handleClient(); // Called in loop()
```

### AsyncWebServer
```cpp
server.on("/path", HTTP_GET, [](AsyncWebServerRequest *request){
  request->send(200, "text/html", "response");
});
server.begin(); // Routes must be registered BEFORE begin()
// No handleClient() needed
```

**Critical Differences:**
1. Async uses `AsyncWebServerRequest*` instead of accessing server directly
2. Routes MUST be registered before `begin()`
3. No `handleClient()` in loop
4. Different authentication mechanism

---

## Implementation Strategy

### Step 1: Study the existing StandardWebServerWrapper
- Understand the interface contract
- Map each method to AsyncWebServer equivalent

### Step 2: Create AsyncWebRequestWrapper
- Wrap `AsyncWebServerRequest*`
- Handle the request/response lifecycle
- Special handling for authentication (no built-in `requestAuthentication()`)

### Step 3: Create AsyncWebServerWrapper
- Store route handlers
- Register routes before begin()
- Handle the timing issue

### Step 4: Extend IotWebConf
- Add constructor accepting `AsyncWebServer*`
- Ensure compatibility with existing flow

### Step 5: Test
- Create minimal example
- Test captive portal
- Test config page

---

## Risks & Challenges

| Challenge | Solution Approach |
|-----------|------------------|
| Routes must be registered before begin() | Store handlers, delay registration |
| No built-in HTTP Basic Auth dialog | Manually send 401 with WWW-Authenticate header |
| Request object lifetime | AsyncWebServerRequest* is managed by library |
| Captive portal redirect | Use request->redirect() |
| CORS handling | Add headers in wrapper |

---

## Success Criteria

- [ ] Code compiles for ESP32 with ESPAsyncWebServer
- [ ] IotWebConf accepts AsyncWebServer* in constructor
- [ ] Captive portal works
- [ ] Config page loads and saves
- [ ] No memory leaks
- [ ] Backward compatibility maintained

---

## Next Steps

1. Analyze StandardWebServerWrapper implementation in detail
2. Create skeleton for AsyncWebServerWrapper
3. Implement AsyncWebRequestWrapper
4. Test incrementally

---

**Status:** STARTED  
**Next Document:** 002-StandardWrapper-Analysis.md
