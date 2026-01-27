# Integration Plan: AsyncWebServer Support in IotWebConf

**Date:** 2026-01-27  
**Document:** 005  
**Phase:** 1B - Integration

---

## Objective

Integrate the AsyncWebServer wrapper into the main IotWebConf class, allowing users to instantiate IotWebConf with an AsyncWebServer instance.

---

## Files to Modify

### 1. `src/IotWebConf.h`
**Changes Required:**
- Include `IotWebConfAsyncWebServer.h` conditionally
- Add AsyncWebServer constructor (conditional compilation)
- Add `AsyncWebServerWrapper` member variable

### 2. `src/IotWebConf.cpp`
**Changes Required:**
- Implement AsyncWebServer constructor
- Follow same pattern as existing WebServer constructor

---

## Implementation Strategy

### Pattern to Follow

The existing implementation shows this delegation pattern:

```cpp
// User-facing constructor (takes concrete WebServer*)
IotWebConf(
    const char* thingName, DNSServer* dnsServer, WebServer* server,
    const char* initialApPassword, const char* configVersion = "init") 
  : IotWebConf(thingName, dnsServer, &this->_standardWebServerWrapper, 
      initialApPassword, configVersion)
{
  this->_standardWebServerWrapper._server = server;
}
```

**Pattern:**
1. Constructor takes concrete server type
2. Delegates to main constructor with wrapper pointer
3. Sets wrapper's internal server pointer

---

## Proposed Changes

### Change 1: IotWebConf.h - Add Include

**Location:** After existing includes, around line 28

```cpp
#include <DNSServer.h> // -- For captive portal

// Include AsyncWebServer wrapper if available
#if defined(ESP_ASYNC_WEB_SERVER_H_) || defined(_ESPAsyncWebServer_H_)
# include <IotWebConfAsyncWebServer.h>
#endif
```

**Reasoning:** Conditional include ensures no dependency when AsyncWebServer isn't used

---

### Change 2: IotWebConf.h - Add Member Variable

**Location:** In private section, after `_standardWebServerWrapper` (around line 617)

```cpp
private:
  // ...existing members...
  StandardWebServerWrapper _standardWebServerWrapper = StandardWebServerWrapper();
  
#if defined(ESP_ASYNC_WEB_SERVER_H_) || defined(_ESPAsyncWebServer_H_)
  AsyncWebServerWrapper _asyncWebServerWrapper = AsyncWebServerWrapper();
#endif
```

**Reasoning:** Parallel to existing StandardWebServerWrapper pattern

---

### Change 3: IotWebConf.h - Add Constructor Declaration

**Location:** After existing constructor (around line 184)

```cpp
IotWebConf(
    const char* thingName, DNSServer* dnsServer, WebServer* server,
    const char* initialApPassword, const char* configVersion = "init") :
    IotWebConf(thingName, dnsServer, &this->_standardWebServerWrapper, 
        initialApPassword, configVersion)
{
  this->_standardWebServerWrapper._server = server;
}

#if defined(ESP_ASYNC_WEB_SERVER_H_) || defined(_ESPAsyncWebServer_H_)
/**
 * Create a new configuration handler with AsyncWebServer support.
 * Use this constructor when you want to share an AsyncWebServer instance
 * between IotWebConf and your application code.
 * 
 * @param thingName - Initial value for the thing name
 * @param dnsServer - A created DNSServer for captive portal
 * @param server - A created AsyncWebServer instance
 * @param initialApPassword - Initial password for AP mode
 * @param configVersion - Configuration version string
 */
IotWebConf(
    const char* thingName, DNSServer* dnsServer, AsyncWebServer* server,
    const char* initialApPassword, const char* configVersion = "init") :
    IotWebConf(thingName, dnsServer, &this->_asyncWebServerWrapper, 
        initialApPassword, configVersion)
{
  this->_asyncWebServerWrapper._server = server;
}
#endif
```

**Reasoning:** 
- Inline implementation (header-only, same as WebServer version)
- Follows exact same delegation pattern
- Conditional compilation keeps clean separation
- Comprehensive documentation for users

---

## No Changes Needed

The following do NOT need modification:
- ✅ Main constructor taking `WebServerWrapper*` - already generic
- ✅ `doLoop()` method - calls `_webServerWrapper->handleClient()` (no-op for async)
- ✅ State management - calls `_webServerWrapper->begin()` (works for both)
- ✅ Route handling - uses `WebRequestWrapper*` interface (works for both)

**Why?** The abstraction layer is already in place and working!

---

## Implementation Steps

1. ✅ Backup current files
2. ⏳ Modify `IotWebConf.h` - add include
3. ⏳ Modify `IotWebConf.h` - add member variable
4. ⏳ Modify `IotWebConf.h` - add constructor
5. ⏳ Compile test (should compile cleanly)
6. ⏳ Create minimal example to test

---

## Testing Checklist

After integration:
- [ ] Code compiles without AsyncWebServer library (backward compat)
- [ ] Code compiles WITH AsyncWebServer library
- [ ] Existing WebServer constructor still works
- [ ] AsyncWebServer constructor accepts server instance
- [ ] Can create IotWebConf object with both server types
- [ ] No warnings or errors during compilation

---

## Risk Mitigation

| Risk | Probability | Impact | Mitigation |
|------|-------------|--------|------------|
| Breaking existing code | Low | High | Conditional compilation, no changes to existing constructors |
| Compiler errors | Low | Medium | Follow exact pattern of existing code |
| Memory overhead | Low | Low | Member only created when used |
| Include order issues | Medium | Low | Use standard include guards |

---

## Expected Outcome

After this integration, users will be able to:

```cpp
// OPTION 1: Existing synchronous WebServer (unchanged)
WebServer server(80);
IotWebConf iotWebConf(thingName, &dnsServer, &server, apPassword);

// OPTION 2: NEW - AsyncWebServer support
AsyncWebServer asyncServer(80);
IotWebConf iotWebConf(thingName, &dnsServer, &asyncServer, apPassword);
```

Both work identically from the user's perspective!

---

## Next Steps After Integration

1. Create example: `examples/IotWebConf19AsyncServer/`
2. Test captive portal functionality
3. Test configuration save/load
4. Test with actual AsyncWebServer routes in user code
5. Document in README.md

---

**Status:** READY TO IMPLEMENT  
**Assigned To:** @dev_senior  
**Estimated Time:** 30 minutes  
**Next Document:** 006-Integration-Complete.md
