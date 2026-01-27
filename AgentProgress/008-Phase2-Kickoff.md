# Phase 2 Kickoff: Enhanced JSON API for Network Management

**Date:** 2026-01-27  
**Document:** 008  
**Phase:** 2 - JSON API Enhancement  
**Status:** 🚀 INITIATED

---

## Phase 2 Objectives

Extend the existing `IotWebConfApi` class to provide complete JSON-based network management capabilities, enabling users to build custom web UIs that can:

1. List all configured WiFi networks
2. Add new networks to the configuration
3. Delete specific networks
4. Connect to a specific stored network
5. Force reconnection attempts
6. Manage multiple WiFi configurations

---

## User Requirements (Original)

> "We want to be able to control IoTWebControl using a json backend, so we can create our own webpages to control the libraries base functions (scan for WiFi, add/connect to a network, list known networks, delete known networks, attempt reconnection)."

---

## Current State Analysis

### What Already Exists ✅

From `src/IotWebConfApi.h/cpp`:

| Endpoint | Method | Function | Status |
|----------|--------|----------|--------|
| `/api/status` | GET | Device and connection status | ✅ Working |
| `/api/scan` | POST | Trigger WiFi scan | ✅ Working |
| `/api/networks` | GET | Get scan results | ✅ Working |
| `/api/wifi` | POST | Connect to network (single) | ⚠️ Limited |
| `/api/config` | GET | Get all config | ✅ Working |
| `/api/config` | POST | Set config | ✅ Working |

### What's Missing ❌

- **List configured networks** - Can't see what networks are already stored
- **Add network to list** - Can only replace primary network
- **Delete specific network** - No way to remove a saved network
- **Connect to specific network** - Can only connect to newly configured one
- **Multiple WiFi support** - API doesn't expose MultipleWifiAddition features

---

## New API Endpoints Required

### 1. GET `/api/networks/configured`
**Purpose:** List all stored WiFi network credentials  
**Response:**
```json
{
  "networks": [
    {
      "id": "iwcWifi0",
      "ssid": "HomeNetwork",
      "isActive": true,
      "hasPassword": true
    },
    {
      "id": "iwcWifi1",
      "ssid": "OfficeNetwork", 
      "isActive": false,
      "hasPassword": true
    }
  ]
}
```

### 2. POST `/api/networks`
**Purpose:** Add new network to configuration  
**Request:**
```json
{
  "ssid": "NewNetwork",
  "password": "password123"
}
```
**Response:**
```json
{
  "status": "added",
  "id": "iwcWifi2"
}
```

### 3. DELETE `/api/networks/{id}`
**Purpose:** Remove a specific network  
**Response:**
```json
{
  "status": "deleted",
  "id": "iwcWifi1"
}
```

### 4. POST `/api/networks/{id}/connect`
**Purpose:** Connect to a specific stored network  
**Response:**
```json
{
  "status": "connecting",
  "ssid": "OfficeNetwork"
}
```

### 5. POST `/api/reconnect`
**Purpose:** Force reconnection with current settings  
**Response:**
```json
{
  "status": "reconnecting"
}
```

---

## Technical Challenges

### Challenge 1: Multiple WiFi Architecture
The library has two modes:
- **Single WiFi:** Uses `WifiParameterGroup _wifiParameters`
- **Multiple WiFi:** Uses `MultipleWifiAddition` with `ChainedWifiParameterGroup[]`

**Solution Needed:** API must detect which mode is active and work with both.

### Challenge 2: Network Identification
How do we identify networks?
- Use parameter group ID (e.g., "iwcWifi0", "iwcWifi1")
- Or use index (0, 1, 2...)?
- Or use SSID (but SSIDs can change)?

**Decision Required:** ID-based is most flexible.

### Challenge 3: Storage Management
- Adding/deleting networks modifies EEPROM
- Must call `saveConfig()` after changes
- Need to manage ParameterGroup lifecycle

### Challenge 4: AsyncWebServer Support
- New endpoints must work with BOTH sync and async servers
- Need URL parameter parsing (e.g., `/api/networks/{id}/connect`)
- AsyncWebServer handles parameters differently

---

## Architecture Approach

### Option A: Extend IotWebConfApi (Recommended)
**Pros:**
- Keeps all API logic together
- Consistent with existing pattern
- Easy to maintain

**Cons:**
- IotWebConfApi becomes larger
- Needs access to MultipleWifiAddition internals

### Option B: Create Separate NetworkManagementApi
**Pros:**
- Separation of concerns
- Cleaner focused class

**Cons:**
- Two API classes to manage
- Duplicated patterns

**Recommendation:** Option A - extend IotWebConfApi

---

## Implementation Plan

### Step 1: Architecture Design (2 hours)
**Assigned to:** @architect  
**Deliverables:**
- Detailed class design for API extensions
- Strategy for single vs multiple WiFi detection
- Network identification scheme
- Storage management approach

### Step 2: Core API Implementation (8 hours)
**Assigned to:** @dev_senior  
**Deliverables:**
- Implement 5 new endpoints in `IotWebConfApi.cpp`
- Helper methods for network management
- Integration with MultipleWifiAddition (if present)
- Error handling

### Step 3: AsyncWebServer Support (4 hours)
**Assigned to:** @dev_senior  
**Deliverables:**
- Async route registration for new endpoints
- URL parameter parsing for async
- Test with both sync and async servers

### Step 4: Example & Testing (4 hours)
**Assigned to:** @dev_junior  
**Deliverables:**
- Update Example 18 or create Example 20
- Demonstrate all new endpoints
- Test both single and multiple WiFi modes

### Step 5: Documentation (2 hours)
**Assigned to:** @librarian  
**Deliverables:**
- Update `doc/JSON-API.md`
- Update example README
- Inline code documentation review

---

## Timeline

| Phase | Duration | Dependencies |
|-------|----------|--------------|
| Architecture Design | 2 hours | None |
| Core Implementation | 8 hours | Architecture complete |
| Async Support | 4 hours | Core complete |
| Example & Testing | 4 hours | Async complete |
| Documentation | 2 hours | All complete |
| **Total** | **20 hours** | Sequential |

**Estimated Completion:** 2-3 days

---

## Team Assignments

### @architect - START NOW
**Task:** Design the network management API architecture  
**See:** Document 009 (next) for your detailed assignment

### @dev_senior - STANDBY
**Task:** Implement core API functionality  
**Wait for:** Architecture design completion  
**Estimated start:** +2 hours

### @dev_junior - STANDBY
**Task:** Create example and test scenarios  
**Wait for:** Core implementation completion  
**Estimated start:** +14 hours

### @librarian - STANDBY
**Task:** Documentation updates  
**Wait for:** Example completion  
**Estimated start:** +18 hours

---

## Success Criteria

- [ ] All 5 new endpoints implemented and working
- [ ] Works with single WiFi mode
- [ ] Works with multiple WiFi mode (if MultipleWifiAddition present)
- [ ] Works with synchronous WebServer
- [ ] Works with AsyncWebServer
- [ ] Example demonstrates all endpoints
- [ ] Documentation updated
- [ ] No breaking changes to existing API
- [ ] Backward compatible

---

## Risk Assessment

| Risk | Probability | Impact | Mitigation |
|------|-------------|--------|------------|
| MultipleWifiAddition integration complexity | High | Medium | Architect to design abstraction layer |
| AsyncWebServer URL param parsing | Medium | Low | Use request->pathArg() method |
| EEPROM write frequency concerns | Low | Medium | Document best practices |
| Breaking existing API | Low | High | Comprehensive testing, no signature changes |

---

## Dependencies

**Required Knowledge:**
- How MultipleWifiAddition works (need to study)
- How to add/remove ParameterGroups dynamically
- EEPROM save/load mechanisms
- URL routing in both sync and async servers

**Code Dependencies:**
- `IotWebConfMultipleWifi.h` - for MultipleWifiAddition
- `IotWebConfApi.cpp` - existing API to extend
- Phase 1 AsyncWebServer wrapper (already complete)

---

## Next Steps

1. **@architect** - I'm creating your detailed design brief now (Document 009)
2. **@manager** - Monitor progress, coordinate handoffs
3. **Team standup** - Check in after architecture phase

---

**Status:** 🚀 PHASE 2 LAUNCHED  
**Next Document:** 009-Architecture-Design-Brief.md  
**Waiting On:** @architect design
