# Example 20B: JSON API - Hybrid Mode (API + Config Portal)

**Target Users:** Developers who want BOTH JSON API and traditional config portal available

## What This Example Shows

This example demonstrates **the key concept** that many users miss: **JSON API and Config Portal share the same storage slots**.

- ✅ JSON API for programmatic network management
- ✅ Traditional config portal (`/config`) for manual management
- ✅ **How both methods share the same storage slots**
- ✅ When to use each method
- ✅ Interactive web UI + config portal integration
- ✅ Real-world hybrid usage scenarios

## Key Concept: Shared Storage

**THIS IS THE MOST IMPORTANT PART TO UNDERSTAND:**

The WiFi credential slots are used by BOTH:
1. **Config Portal** (`/config`) - Users fill forms in browser
2. **JSON API** (`POST /api/networks`) - Apps send REST requests

**They're the same slots!** When you add a network via the API, it appears in the config portal. When you add a network via the config portal, it appears in API responses.

```
┌─────────────────────────────────────────────┐
│         SHARED WIFI STORAGE SLOTS           │
│                                             │
│  iwcWifi0 (primary) ─┐                     │
│  wifi2 (slot 1)       ├─ Accessible by:    │
│  wifi3 (slot 2)       │  • Config Portal    │
│  wifi4 (slot 3)      ─┘  • JSON API        │
│                                             │
│  Both methods read/write the SAME slots!   │
└─────────────────────────────────────────────┘
```

### Example: Adding Network via Config Portal

1. User goes to `http://192.168.1.100/config`
2. User fills in "WiFi 2 SSID" field with "HomeNetwork"
3. User fills in "WiFi 2 Password" field
4. User clicks "Apply"
5. **Credentials saved to `wifi2` slot**

### Example: Adding Network via JSON API

```bash
curl -X POST http://192.168.1.100/api/networks \
  -H "Content-Type: application/json" \
  -d '{"ssid":"OfficeNetwork","password":"office123"}'
```

Response: `{"status":"connecting","id":"wifi3","slot":2}`

**Credentials saved to `wifi3` slot** (next available)

### Example: Both Methods Working Together

```bash
# Check what's configured
curl http://192.168.1.100/api/networks/configured

# Response shows BOTH networks:
{
  "networks": [
    {"id":"iwcWifi0","ssid":"PrimaryWiFi",...},
    {"id":"wifi2","ssid":"HomeNetwork",...},      ← Added via portal
    {"id":"wifi3","ssid":"OfficeNetwork",...}     ← Added via API
  ]
}
```

**Both show up in Config Portal AND in API responses!**

## When to Use This Example

✅ Use this hybrid example if:
- End users need a simple web form to add networks (non-technical users)
- Your app also needs to add networks programmatically (IoT automation)
- You want maximum flexibility (both manual and automated)
- You're building a product for diverse users (some technical, some not)
- You need backwards compatibility with config portal

❌ Don't use this if:
- You ONLY want JSON API (use Example 20A instead - cleaner)
- You never want users touching the config portal
- You're building a pure API-driven device
- You want to hide the config portal from users

## When to Use Each Method

### Use Config Portal When:
- 👤 End user is configuring the device manually
- 🏠 Initial setup by non-technical user
- 🛠️ Admin wants visual interface
- 📱 Mobile device configuration (form-based)
- 🔧 Troubleshooting or support scenarios

### Use JSON API When:
- 🤖 Automation scripts provisioning devices
- 📱 Mobile app managing device
- ☁️ Cloud service adding networks remotely
- 🔄 Batch operations (multiple networks at once)
- 🧪 Testing and development

## Usage Scenarios

### Scenario 1: User Uses Config Portal

**Steps:**
1. User opens browser to `http://192.168.1.100/config`
2. User sees form with fields:
   - WiFi SSID (primary)
   - WiFi Password (primary)
   - **WiFi 2 SSID** ← Additional slot
   - **WiFi 2 Password**
   - **WiFi 3 SSID** ← Additional slot
   - **WiFi 3 Password**
   - etc.
3. User fills in "WiFi 2 SSID" = "HomeNetwork"
4. User fills in "WiFi 2 Password" = "home123"
5. User clicks "Apply"
6. ✅ Credentials saved to `wifi2` slot

**Result:**
```bash
# API now returns this network
curl http://192.168.1.100/api/networks/configured

{
  "networks": [
    {"id":"wifi2","ssid":"HomeNetwork","isActive":true,...}
  ]
}
```

### Scenario 2: App Uses JSON API

**Steps:**
```bash
# Your app sends this
curl -X POST http://192.168.1.100/api/networks \
  -H "Content-Type: application/json" \
  -d '{"ssid":"OfficeNetwork","password":"office123"}'
```

**Response:**
```json
{
  "status": "connecting",
  "id": "wifi3",
  "slot": 2,
  "ssid": "OfficeNetwork"
}
```

**Result:**
- ✅ Credentials saved to `wifi3` slot (next available)
- ✅ Shows up in Config Portal form
- ✅ Shows up in API responses

### Scenario 3: User Then Checks Config Portal

1. User opens `http://192.168.1.100/config`
2. User sees **BOTH** networks:
   - WiFi 2 SSID: `HomeNetwork` (they added this)
   - WiFi 3 SSID: `OfficeNetwork` (app added this!)
3. User can edit or delete either network
4. Changes sync back to API

### Scenario 4: App Deletes Network via API

```bash
# App deletes network
curl -X DELETE http://192.168.1.100/api/networks/wifi3
```

**Result:**
- ✅ Credentials cleared from `wifi3` slot
- ✅ Config Portal shows empty "WiFi 3 SSID" field
- ✅ API no longer returns this network

### Scenario 5: Mixed Operations

**Timeline:**
1. **User** adds "HomeWiFi" via config portal → saved to `wifi2`
2. **App** adds "GuestWiFi" via API → saved to `wifi3`
3. **User** adds "OfficeWiFi" via config portal → saved to `wifi4`
4. **App** deletes "GuestWiFi" via API → `wifi3` slot now empty
5. **App** adds "CafeWiFi" via API → saved to `wifi3` (reuses empty slot)

**All operations work seamlessly because they share the same storage!**

## Hardware Requirements

- **ESP8266** or **ESP32** development board
- Optional: LED on `LED_BUILTIN` for status indication
- Optional: Button on GPIO 0 for configuration reset

## Software Requirements

### Build Flags

Add this to your `platformio.ini`:

```ini
build_flags = 
  -D IOTWEBCONF_ENABLE_JSON
```

Or in Arduino IDE, the example automatically defines it.

### Libraries

- **IotWebConf** (this library)
- **ArduinoJson** v6+
- **ESP8266WiFi** or **WiFi** (included with ESP core)
- **DNSServer** (included with ESP core)
- **WebServer** or **ESP8266WebServer** (included with ESP core)

## Setup and Usage

### 1. Initial Setup

1. Flash the example to your device
2. Device creates AP: **HybridNetMgr** with password `hybrid123`
3. Connect to the AP
4. Device redirects to config portal
5. Fill in your primary WiFi network
6. Device connects and starts all services

### 2. Access Points

Once connected to WiFi, you have THREE ways to manage networks:

#### Option 1: Config Portal (Manual)
```
http://192.168.1.100/config
```
- Traditional web form
- Fill in SSID/password fields
- Click "Apply" to save
- Good for: End users, initial setup, manual config

#### Option 2: JSON API (Programmatic)
```
http://192.168.1.100/api/networks
```
- RESTful JSON API
- POST/GET/DELETE operations
- Auto-slot assignment
- Good for: Apps, automation, scripts

#### Option 3: Web UI (Interactive)
```
http://192.168.1.100/
```
- Modern web interface
- Uses JSON API internally
- Shows all configured networks
- One-click operations
- Good for: Visual management, status monitoring

## API Usage Examples

### List Configured Networks

```bash
curl http://192.168.1.100/api/networks/configured
```

**Response:**
```json
{
  "networks": [
    {
      "id": "iwcWifi0",
      "ssid": "PrimaryWiFi",
      "isActive": true,
      "hasPassword": true,
      "isCurrent": true
    },
    {
      "id": "wifi2",
      "ssid": "HomeNetwork",
      "isActive": true,
      "hasPassword": true,
      "isCurrent": false
    },
    {
      "id": "wifi3",
      "ssid": "",
      "isActive": false,
      "hasPassword": false,
      "isCurrent": false
    }
  ]
}
```

### Add Network (Auto-Slot Assignment)

```bash
curl -X POST http://192.168.1.100/api/networks \
  -H "Content-Type: application/json" \
  -d '{"ssid":"NewNetwork","password":"newpass123"}'
```

**Response:**
```json
{
  "status": "connecting",
  "id": "wifi3",
  "slot": 2,
  "ssid": "NewNetwork"
}
```

**What happens:**
- ✅ API finds next empty slot (`wifi3`)
- ✅ Saves credentials to EEPROM
- ✅ Attempts connection
- ✅ Network now visible in config portal at `/config`

### Delete Network

```bash
curl -X DELETE http://192.168.1.100/api/networks/wifi2
```

**Response:**
```json
{
  "status": "deleted",
  "id": "wifi2"
}
```

**What happens:**
- ✅ Credentials cleared from `wifi2` slot
- ✅ Config portal shows empty fields for WiFi 2
- ✅ Slot available for future networks

**Note:** Cannot delete primary network (`iwcWifi0`).

### Connect to Specific Network

```bash
curl -X POST http://192.168.1.100/api/networks/wifi2/connect
```

**Response:**
```json
{
  "status": "connecting",
  "id": "wifi2",
  "ssid": "HomeNetwork"
}
```

### Force Reconnection

```bash
curl -X POST http://192.168.1.100/api/reconnect
```

**Response:**
```json
{
  "status": "reconnecting"
}
```

## Config Portal Usage

### Adding Network via Portal

1. Open browser to `http://192.168.1.100/config`
2. Scroll to WiFi configuration section
3. Find empty slot (e.g., "WiFi 2 SSID")
4. Enter SSID: `MyNetwork`
5. Enter password: `mypass123`
6. Click "Apply"
7. Wait for device to reconnect
8. ✅ Network saved to `wifi2` slot

### Verifying via API

```bash
curl http://192.168.1.100/api/networks/configured
```

You'll see the network you just added via config portal!

### Editing Network via Portal

1. Open `http://192.168.1.100/config`
2. Find the network field
3. Change SSID or password
4. Click "Apply"
5. ✅ Changes saved and visible via API

### Deleting Network via Portal

**Option 1: Clear the fields**
1. Open `http://192.168.1.100/config`
2. Clear the SSID field (leave empty)
3. Click "Apply"
4. ✅ Network removed

**Option 2: Use API**
```bash
curl -X DELETE http://192.168.1.100/api/networks/wifi2
```

## Code Structure

### Key Components

```cpp
// ═══════════════════════════════════════════════════════
// WIFI STORAGE SLOTS (SHARED BY API AND CONFIG PORTAL)
// ═══════════════════════════════════════════════════════

#define NUM_WIFI_SLOTS 3  // 3 additional networks

iotwebconf::ChainedWifiParameterGroup wifiSlots[NUM_WIFI_SLOTS] = {
  {"wifi2"},  // Slot 1: Second WiFi network
  {"wifi3"},  // Slot 2: Third WiFi network
  {"wifi4"}   // Slot 3: Fourth WiFi network
};

// Multiple WiFi addition (makes slots available to both portal and API)
iotwebconf::MultipleWifiAddition multiWifi(
  &iotWebConf,
  wifiSlots,
  NUM_WIFI_SLOTS
);

// Setup
void setup() {
  // Initialize multi-wifi (enables shared slots)
  multiWifi.init();
  
  // Give API access to multi-wifi slots
  api.setMultipleWifiAddition(&multiWifi);
  
  // Register API routes
  api.registerRoutes();
}
```

### How Shared Storage Works

1. **MultipleWifiAddition** creates parameter groups for additional WiFi slots
2. **IotWebConf** shows these as form fields in config portal
3. **IotWebConfApi** accesses the same parameter groups via API
4. **All changes saved to same EEPROM locations**

```
User fills form → ChainedWifiParameterGroup → EEPROM
                            ↑
API saves JSON → ChainedWifiParameterGroup → EEPROM
                            ↑
                        Same object!
```

## Network Slot Reference

| Slot ID | Index | Description | Can Delete? | Access Methods |
|---------|-------|-------------|-------------|----------------|
| `iwcWifi0` | 0 | Primary WiFi | ❌ No | Config Portal only |
| `wifi2` | 1 | Second WiFi | ✅ Yes | Both API & Portal |
| `wifi3` | 2 | Third WiFi | ✅ Yes | Both API & Portal |
| `wifi4` | 3 | Fourth WiFi | ✅ Yes | Both API & Portal |

### Slot Assignment Logic

When you POST to `/api/networks` without specifying a slot:

1. API scans `wifi2`, `wifi3`, `wifi4` in order
2. Finds first slot with empty SSID
3. Assigns network to that slot
4. Returns slot ID in response

**Example:**
```bash
# wifi2 is empty, wifi3 has "Office", wifi4 is empty

POST /api/networks {"ssid":"NewNet","password":"pass"}

# Response: {"id":"wifi2","slot":1,...}  ← Uses wifi2
```

## Customization

### Add More Slots

```cpp
#define NUM_WIFI_SLOTS 5  // Increase to 5

iotwebconf::ChainedWifiParameterGroup wifiSlots[NUM_WIFI_SLOTS] = {
  {"wifi2"},
  {"wifi3"},
  {"wifi4"},
  {"wifi5"},  // Add more slots
  {"wifi6"}
};
```

**Result:**
- Config portal shows 5 additional WiFi fields
- API can assign to any of 5 slots
- Total capacity: 6 networks (1 primary + 5 additional)

### Custom Slot Names

```cpp
iotwebconf::ChainedWifiParameterGroup wifiSlots[NUM_WIFI_SLOTS] = {
  {"home"},    // Custom ID
  {"office"},  // Custom ID
  {"guest"}    // Custom ID
};
```

**API usage:**
```bash
# Delete using custom ID
curl -X DELETE http://192.168.1.100/api/networks/home
```

### Disable Config Portal

If you want API-only mode (defeats the purpose of hybrid, but possible):

```cpp
void setup() {
  // Don't register /config route
  // server.on("/config", []{ iotWebConf.handleConfig(); });
  
  // Users can only use API, not portal
}
```

See Example 20A for proper API-only implementation.

## Troubleshooting

### "Network not found" Error

**Symptom:** API returns `"error": "Network not found"`

**Causes:**
- Network ID doesn't exist (typo in ID)
- MultipleWifiAddition not enabled
- Trying to access slot that wasn't created

**Solution:**
```bash
# Check available slots
curl http://192.168.1.100/api/networks/configured

# Use exact ID from response
curl -X DELETE http://192.168.1.100/api/networks/wifi2
```

### "No available network slots" Error

**Symptom:** Can't add network, all slots full

**Solution:**
1. Delete unused network via API:
   ```bash
   curl -X DELETE http://192.168.1.100/api/networks/wifi3
   ```

2. Or delete via config portal (clear SSID field)

3. Or increase `NUM_WIFI_SLOTS` in code and reflash

### Changes in Config Portal Not Showing in API

**Symptom:** Add network via config portal, doesn't show in API response

**Cause:** Didn't click "Apply" button

**Solution:**
1. Make changes in config portal
2. **Click "Apply"** button at bottom
3. Wait for device to save (LED stops blinking)
4. Now query API:
   ```bash
   curl http://192.168.1.100/api/networks/configured
   ```

### Changes via API Not Showing in Config Portal

**Symptom:** Add network via API, doesn't show in config portal form

**Cause:** Browser caching old config page

**Solution:**
1. Hard refresh browser (Ctrl+F5 or Cmd+Shift+R)
2. Or close browser and reopen
3. Or use incognito/private mode

### "Cannot delete primary network" Error

**Symptom:** `DELETE /api/networks/iwcWifi0` fails

**This is intentional!**
- Primary network (`iwcWifi0`) cannot be deleted via API
- Prevents locking yourself out of device
- To change primary network, use config portal

### EEPROM Wear Concerns

**Issue:** Worried about EEPROM wear from frequent writes

**Facts:**
- EEPROM rated for ~100,000 write cycles
- Each add/delete writes to EEPROM
- Normal usage is fine (not millions of operations)

**Best Practices:**
- Don't call add/delete in loops
- Don't auto-add networks on every boot
- Use POST `/api/reconnect` when you just want to reconnect (doesn't write EEPROM)

**Example of BAD code:**
```cpp
void loop() {
  // DON'T DO THIS! Wears out EEPROM
  addNetwork("test", "pass");
  delay(1000);
}
```

**Example of GOOD code:**
```cpp
void setup() {
  // Add networks once during setup - OK
  if (needsProvisioning()) {
    addNetworkViaAPI("network", "pass");
  }
}

void loop() {
  // Just reconnect if needed - doesn't write EEPROM
  if (wifiDisconnected && retryNeeded) {
    reconnectViaAPI();
  }
}
```

## Security Considerations

⚠️ **This example does NOT include authentication!**

**What's exposed:**
- Anyone on your WiFi can access `/config`
- Anyone on your WiFi can call API endpoints
- Passwords visible in config portal to anyone
- No rate limiting on API calls

**For production, you MUST add:**

### 1. HTTP Basic Authentication

```cpp
server.on("/config", []() {
  if (!server.authenticate("admin", "password")) {
    return server.requestAuthentication();
  }
  iotWebConf.handleConfig();
});

server.on("/api/networks", HTTP_POST, []() {
  if (!server.authenticate("api_user", "api_password")) {
    return server.requestAuthentication();
  }
  api.handleApiAddNetwork();
});
```

### 2. API Keys

```cpp
bool validateApiKey() {
  String apiKey = server.header("X-API-Key");
  return apiKey == "your-secret-api-key";
}

server.on("/api/networks", HTTP_POST, []() {
  if (!validateApiKey()) {
    api.sendJson(401, {{"error", "Unauthorized"}});
    return;
  }
  api.handleApiAddNetwork();
});
```

### 3. HTTPS (TLS)

For ESP32:
```cpp
#include <WiFiClientSecure.h>
#include <WebServer.h>

WebServerSecure server(443);
// Add certificate and key
```

### 4. Rate Limiting

```cpp
unsigned long lastApiCall = 0;
const unsigned long API_RATE_LIMIT = 5000; // 5 seconds

bool checkRateLimit() {
  unsigned long now = millis();
  if (now - lastApiCall < API_RATE_LIMIT) {
    return false;
  }
  lastApiCall = now;
  return true;
}
```

## Comparison: Hybrid vs API-Only

| Feature | Example 20B (Hybrid) | Example 20A (API-Only) |
|---------|---------------------|------------------------|
| Config Portal | ✅ Yes | ❌ No |
| JSON API | ✅ Yes | ✅ Yes |
| Web UI | ✅ Yes | ✅ Yes |
| User-friendly | ✅ Very | ⚠️ Technical users only |
| Code Complexity | ⚠️ Medium | ✅ Simple |
| Use Case | Production devices | Developer tools |
| Non-technical users | ✅ Great | ❌ Confusing |
| API automation | ✅ Great | ✅ Great |

**Choose Hybrid (20B) when:**
- Building consumer products
- Non-technical users need access
- Want backwards compatibility
- Need flexibility

**Choose API-Only (20A) when:**
- Building developer tools
- All users are technical
- Want simplest possible code
- Don't want config portal clutter

## Related Examples

- **IotWebConf20AJsonApiOnly** - Pure JSON API (no config portal)
- **IotWebConf20NetworkManagement** - Original (now deprecated, use 20A or 20B)
- **IotWebConf18JsonApi** - Basic JSON API introduction
- **IotWebConf15MultipleWifi** - Multiple WiFi without API

## Migration Guide

### From Example 20 (Original)

The original Example 20 (`IotWebConf20NetworkManagement`) is now split into:
- **20A** - API-only mode
- **20B** - Hybrid mode (this example)

**If you were using Example 20:**
- Keep using it (still works)
- Or migrate to 20B if you want clearer documentation
- Code is nearly identical

### From Example 20A to 20B

**To add config portal to API-only setup:**

1. Keep existing `MultipleWifiAddition` setup
2. Add config portal route:
   ```cpp
   server.on("/config", []{ iotWebConf.handleConfig(); });
   ```
3. That's it! Portal and API now share storage

### From Example 15 (MultipleWifi) to 20B

**To add JSON API to config-portal setup:**

1. Enable JSON: `#define IOTWEBCONF_ENABLE_JSON`
2. Include API: `#include <IotWebConfApi.h>`
3. Create API instance: `IotWebConfApi api(&iotWebConf, &server);`
4. Link to MultiWifi: `api.setMultipleWifiAddition(&multiWifi);`
5. Register routes: `api.registerRoutes();`
6. Done! API and portal now work together

## Real-World Use Cases

### Use Case 1: Smart Home Device

**Scenario:** WiFi-enabled thermostat

**Requirements:**
- Homeowner needs simple setup (config portal)
- Home automation system needs programmatic control (API)

**Solution:** Use Example 20B
- Homeowner uses config portal during installation
- Home Assistant uses API to add guest network when guests arrive
- Both methods available, both work

### Use Case 2: IoT Gateway

**Scenario:** Industrial IoT gateway in factory

**Requirements:**
- Technician configures primary network manually
- Cloud management system adds backup networks remotely

**Solution:** Use Example 20B
- Technician uses tablet/phone with config portal
- Cloud system uses API to provision backup connections
- Factory network and cellular backup both configured

### Use Case 3: Hotel WiFi Device

**Scenario:** Device that needs to connect to hotel WiFi

**Requirements:**
- Traveler uses config portal to add hotel WiFi
- Mobile app also able to add networks

**Solution:** Use Example 20B
- User opens config portal when at hotel
- Mobile app can also add networks via API
- Device has multiple saved networks for different hotels

### Use Case 4: Retail Display

**Scenario:** Digital signage in retail store

**Requirements:**
- Store staff uses simple interface (not technical)
- Corporate IT can provision remotely

**Solution:** Use Example 20B
- Store staff: "Just go to the config page and type the WiFi name"
- Corporate IT: Uses API to provision all stores from central system

## Best Practices

### 1. Document Which Method to Use

Tell your users which method to use:

**For end users:**
> "To add a WiFi network, open http://device-ip/config and fill in the form"

**For developers:**
> "To add networks programmatically, POST to /api/networks endpoint"

### 2. Provide Both Options in UI

The web UI in this example does exactly this:
```html
<button type="submit">➕ Add via API</button>
<button onclick="location.href='/config'">⚙️ Or Use Config Portal</button>
```

### 3. Explain Shared Storage

If users are confused, explain:
> "The WiFi networks are stored in device memory. You can add them through the
> settings page OR through the app - both work the same way!"

### 4. Validate Before Saving

```cpp
// In your API handler
if (ssid.length() == 0) {
  return sendError("SSID cannot be empty");
}

if (ssid.length() > IOTWEBCONF_WORD_LEN) {
  return sendError("SSID too long");
}
```

### 5. Provide Feedback

```cpp
// Good: Tell user what happened
Serial.println("Network added via API to slot wifi2");

// Better: Also tell them it's in config portal
Serial.println("Network added to wifi2 - visible in config portal at /config");
```

## Testing Checklist

When implementing hybrid mode, test these scenarios:

- [ ] Add network via config portal → Shows in API response
- [ ] Add network via API → Shows in config portal fields
- [ ] Delete network via API → Config portal fields cleared
- [ ] Delete network via portal (clear SSID) → API no longer returns it
- [ ] Edit network via portal → API returns updated values
- [ ] Connect via API → Device connects successfully
- [ ] Multiple users adding networks simultaneously
- [ ] Fill all slots → API returns "no available slots"
- [ ] Delete from middle slot → Next API add uses that slot
- [ ] Device reboot → All networks persist
- [ ] Factory reset → All networks cleared

## License

MIT License - See LICENSE file in the repository root.

## Contributing

This is part of the IotWebConf library by Balazs Kelemen.

- Repository: https://github.com/prampec/IotWebConf
- Issues: https://github.com/prampec/IotWebConf/issues
- Discussions: https://github.com/prampec/IotWebConf/discussions

## Support

For questions about this hybrid example:

1. Read this README thoroughly
2. Check the "Troubleshooting" section
3. Review Example 20A for API-only comparison
4. Check GitHub Issues
5. Ask in GitHub Discussions

## API Documentation

For complete API reference:
- `doc/JSON-API.md` - Full API specification
- `doc/JSON-API-PHASE3.md` - Phase 3 features (add/delete)

## Changelog

- **v1.0** - Initial hybrid example
  - Shared storage between portal and API
  - Comprehensive documentation
  - Real-world usage scenarios
