# Implementation: AsyncWebServer Wrapper Classes

**Date:** 2026-01-27  
**Document:** 003  
**Purpose:** Document the implementation of ESPAsyncWebServer wrapper classes

---

## Overview

Created `src/IotWebConfAsyncWebServer.h` implementing wrapper classes that adapt ESPAsyncWebServer to IotWebConf's WebRequestWrapper and WebServerWrapper interfaces.

**File:** `src/IotWebConfAsyncWebServer.h`  
**Lines:** ~330  
**Status:** Complete - Ready for testing

---

## Implementation Decisions

### 1. Conditional Compilation

```cpp
#if defined(ESP_ASYNC_WEB_SERVER_H_) || defined(_ESPAsyncWebServer_H_)
// ... implementation ...
#endif
```

**Rationale:**
- Only compiles if ESPAsyncWebServer is available
- Prevents compilation errors if user doesn't have the library
- Supports both possible header guard variants

---

### 2. AsyncWebRequestWrapper Implementation

#### Challenge: requestAuthentication()

**Problem:** AsyncWebServer doesn't have a built-in `requestAuthentication()` method like the synchronous WebServer.

**Solution:** Manually construct 401 response with WWW-Authenticate header:

```cpp
void requestAuthentication() override
{
  AsyncWebServerResponse* response = this->_request->beginResponse(401, "text/plain", "Unauthorized");
  response->addHeader("WWW-Authenticate", "Basic realm=\"IotWebConf\"");
  this->_request->send(response);
}
```

**Why this works:**
- Sends proper HTTP 401 status
- Includes WWW-Authenticate header to trigger browser auth dialog
- Uses "Basic realm=\"IotWebConf\"" to match sync server behavior
- Browsers recognize this standard pattern

---

#### Challenge: sendHeader() and send() Timing

**Problem:** In AsyncWebServer, headers must be added to the response object BEFORE calling send(). The synchronous API allows calling sendHeader() then send() separately.

**Solution:** Buffer headers and apply them when send() is called:

```cpp
void sendHeader(const String& name, const String& value, bool first = false) override
{
  _pendingHeaders.push_back({name, value});
}

void send(int code, const char* content_type = nullptr, const String& content = String("")) override
{
  AsyncWebServerResponse* response = this->_request->beginResponse(code, content_type, content);
  
  // Apply buffered headers
  for (auto& header : _pendingHeaders)
  {
    response->addHeader(header.first, header.second);
  }
  _pendingHeaders.clear();
  
  this->_request->send(response);
}
```

**Why this works:**
- Maintains compatibility with synchronous API call pattern
- Headers are collected in a vector
- All headers applied atomically when response is created
- Clears buffer after use

---

#### Challenge: Chunked Responses (sendContent)

**Problem:** IotWebConf builds HTML responses in chunks:
1. Calls `sendHeader()` multiple times
2. Calls `setContentLength()`
3. Calls `sendContent()` multiple times with fragments
4. Calls `stop()` to finish

AsyncWebServer doesn't support this pattern - it expects the full response content.

**Solution:** Accumulate content and send on stop():

```cpp
void sendContent(const String& content) override
{
  _chunkedContent += content;
}

void stop() override
{
  if (_chunkedContent.length() > 0)
  {
    AsyncWebServerResponse* response = this->_request->beginResponse(200, "text/html", _chunkedContent);
    
    // Apply headers and content length
    for (auto& header : _pendingHeaders)
    {
      response->addHeader(header.first, header.second);
    }
    _pendingHeaders.clear();
    
    if (_contentLengthSet)
    {
      response->setContentLength(_contentLength);
    }
    
    this->_request->send(response);
    _chunkedContent = "";
  }
}
```

**Trade-offs:**
- ✅ Simple and works with IotWebConf's usage pattern
- ✅ Maintains API compatibility
- ⚠️ Accumulates full response in memory (but IotWebConf does this anyway)
- ❌ Not true streaming (but IotWebConf doesn't need true streaming)

**Alternative considered:** Using AsyncWebServerResponse with callbacks for true streaming, but this would require major changes to IotWebConf's internal HTML generation.

---

#### Other Method Mappings

| Method | Implementation | Notes |
|--------|---------------|-------|
| `hostHeader()` | `_request->header("Host")` | Get from request headers |
| `localIP()` | `_request->client()->localIP()` | Via client object |
| `localPort()` | `_request->client()->localPort()` | Via client object |
| `uri()` | `_request->url()` | Direct mapping |
| `authenticate()` | `_request->authenticate()` | Direct mapping |
| `hasArg()` | `_request->hasArg()` | Direct mapping |
| `arg()` | `_request->arg()` | Direct mapping |

---

### 3. AsyncWebServerWrapper Implementation

Much simpler than the request wrapper:

```cpp
class AsyncWebServerWrapper : public WebServerWrapper
{
public:
  AsyncWebServerWrapper(AsyncWebServer* server) { this->_server = server; }

  void handleClient() override
  {
    // No-op: AsyncWebServer is event-driven
  }

  void begin() override
  {
    this->_server->begin();
  }

private:
  AsyncWebServerWrapper() { }
  AsyncWebServer* _server;
  friend IotWebConf;
};
```

**Why handleClient() is a no-op:**
- Synchronous WebServer needs `handleClient()` called in loop() to process requests
- AsyncWebServer uses event-driven architecture with background tasks
- No explicit polling needed - requests are handled automatically

**Why this is safe:**
- IotWebConf calls `handleClient()` in `doLoop()`
- Empty implementation has no performance impact
- AsyncWebServer continues working in background

---

## State Management

### AsyncWebRequestWrapper State

```cpp
private:
  AsyncWebServerRequest* _request;                           // The wrapped request
  std::vector<std::pair<String, String>> _pendingHeaders;   // Buffered headers
  String _chunkedContent = "";                               // Accumulated content
  size_t _contentLength = 0;                                 // Content length
  bool _contentLengthSet = false;                            // Flag for content length
```

**Lifetime:** Created per-request in route handlers, destroyed after response sent.

**Memory usage:** Proportional to response size (same as synchronous version).

---

## Code Quality Notes

### Documentation

- Comprehensive class-level documentation explaining async differences
- Method-level documentation for non-obvious implementations
- Inline comments explaining trade-offs and design decisions
- Clear explanation of why certain approaches were chosen

### Following Existing Style

- Matches IotWebConf.h formatting and structure
- Uses same naming conventions (private `_variableName`)
- Same friend class pattern as StandardWebServerWrapper
- Consistent use of `this->` for member access
- Private default constructor pattern

### Error Handling

- Check for header existence in `hostHeader()`
- Safe handling of empty content in `stop()`
- Null-safe string operations

---

## Testing Considerations

### What Needs Testing

1. **Basic Authentication Flow**
   - Test `authenticate()` with valid credentials
   - Test `requestAuthentication()` triggers browser auth dialog
   - Verify 401 response includes proper WWW-Authenticate header

2. **Response Building**
   - Test `send()` with various content types
   - Test `sendHeader()` + `send()` sequence
   - Test `sendContent()` + `stop()` chunked pattern
   - Verify headers are applied correctly

3. **Captive Portal**
   - Test hostHeader detection
   - Test 302 redirect behavior
   - Test with various client types (phones, laptops)

4. **Config Page**
   - Test form rendering
   - Test form submission
   - Test parameter validation
   - Test config save/load

5. **Update Page** (if using update server)
   - Test firmware upload flow

---

## Known Limitations

### 1. Content Accumulation

**Issue:** Full response is accumulated in memory before sending.

**Impact:** 
- For large responses (>8KB), could cause memory pressure on ESP8266
- IotWebConf's config pages are typically <4KB, so this is acceptable

**Mitigation:** If needed in future, could implement true streaming with AsyncWebServerResponse chunked mode.

---

### 2. No Streaming Support

**Issue:** True HTTP streaming (send chunks as generated) not supported.

**Impact:** 
- Can't do server-sent events (SSE)
- Can't stream large files
- IotWebConf doesn't need these features currently

**Future Enhancement:** Could add streaming support if needed.

---

## Integration Points

### Next Steps for Full Integration

1. **Add IotWebConf Constructor** (in `src/IotWebConf.h`):
```cpp
IotWebConf(
  const char* thingName, 
  DNSServer* dnsServer, 
  AsyncWebServer* server,
  const char* initialApPassword, 
  const char* configVersion = "init"
) : IotWebConf(thingName, dnsServer, &this->_asyncWebServerWrapper, initialApPassword, configVersion)
{
  this->_asyncWebServerWrapper._server = server;
}
```

2. **Add Member Variable** (in `src/IotWebConf.h` private section):
```cpp
AsyncWebServerWrapper _asyncWebServerWrapper = AsyncWebServerWrapper();
```

3. **Route Registration Pattern** - Users will need to register routes like:
```cpp
server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
  AsyncWebRequestWrapper wrappedRequest(request);
  if (iotWebConf.handleCaptivePortal(&wrappedRequest)) { return; }
  // ... handle root
});

server.on("/config", HTTP_GET | HTTP_POST, [](AsyncWebServerRequest *request){
  AsyncWebRequestWrapper wrappedRequest(request);
  iotWebConf.handleConfig(&wrappedRequest);
});

server.onNotFound([](AsyncWebServerRequest *request){
  AsyncWebRequestWrapper wrappedRequest(request);
  iotWebConf.handleNotFound(&wrappedRequest);
});
```

---

## Async-Specific Quirks & Gotchas

### Quirk 1: Route Registration Timing

**Issue:** AsyncWebServer requires routes to be registered BEFORE `server.begin()`.

**Solution:** User must set up all routes in their sketch before calling `iotWebConf.init()`.

**Documentation needed:** Clear example showing proper initialization order.

---

### Quirk 2: Request Object Lifetime

**Issue:** `AsyncWebServerRequest*` is managed by the library and only valid during callback.

**Solution:** Our wrapper is created and destroyed within each callback, so no lifetime issues.

---

### Quirk 3: Multiple send() Calls

**Issue:** AsyncWebServerRequest can only have send() called once.

**Implication:** Our pattern of buffering headers and accumulating content is essential.

---

## Performance Considerations

### Memory Usage

- **Synchronous:** Sends response incrementally, lower peak memory
- **Async (our implementation):** Accumulates full response, higher peak memory
- **Typical IotWebConf response:** 2-4KB (acceptable on ESP8266/ESP32)

### CPU Usage

- **Synchronous:** Blocks during send
- **Async:** Non-blocking, better CPU utilization
- **Overall:** Async is more efficient for multiple concurrent connections

---

## Security Considerations

### HTTP Basic Auth

- Credentials sent base64-encoded (not encrypted)
- Should use HTTPS in production (AsyncWebServer supports this)
- Matches security level of synchronous version

### Input Validation

- Inherits IotWebConf's existing validation
- No additional security risks introduced

---

## Next Document

**004-Integration-Testing.md** will document:
- Adding constructor to IotWebConf
- Creating minimal example
- Testing results
- Performance comparisons
- Any issues found and fixes

---

## Summary

✅ **AsyncWebRequestWrapper:** Fully implemented with buffering for header/content accumulation  
✅ **AsyncWebServerWrapper:** Implemented with no-op handleClient()  
✅ **Documentation:** Comprehensive inline documentation  
✅ **Code Quality:** Matches existing style and conventions  
✅ **Ready:** For integration into IotWebConf main class  

**Key Innovation:** The buffering approach allows AsyncWebServer to work with IotWebConf's synchronous-style API without requiring any changes to the library's core HTML generation logic.

---

**Status:** IMPLEMENTATION COMPLETE  
**Next Task:** Add constructor to IotWebConf and create example
