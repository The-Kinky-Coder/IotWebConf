# IotWebConf20NetworkManagement

This example demonstrates the **Phase 3 Network Management JSON API** added to IotWebConf.

## Features Demonstrated

### New Phase 3 API Endpoints

1. **GET /api/networks/configured**
   - Lists all configured WiFi network credentials
   - Shows which network is currently connected
   - Displays active/inactive status for each network

2. **POST /api/networks** ⭐ NEW in Phase 3
   - Add network credentials with **automatic slot assignment**
   - Finds next available slot automatically
   - Returns assigned slot number and connection status
   - Saves changes to EEPROM

3. **DELETE /api/networks/{id}** ⭐ NEW in Phase 3
   - Delete credentials from a specific network slot
   - Clears SSID and password (sets to empty)
   - Cannot delete primary network (iwcWifi0)
   - Saves changes to EEPROM

4. **POST /api/networks/{id}/connect**
   - Connect to a specific saved network
   - Copies network credentials to primary slot
   - Triggers immediate connection attempt

5. **POST /api/reconnect**
   - Force WiFi reconnection with current settings
   - Useful after configuration changes
   - Does not modify EEPROM

### Multiple WiFi Support

This example optionally demonstrates **MultipleWifiAddition**:
- Manage up to 3 WiFi networks (easily extendable)
- Switch between networks via API
- Fallback to alternate networks on connection failure

### Interactive Web UI ⭐ FULLY FUNCTIONAL

Includes a complete, tested single-page web application:
- **Dashboard**: Shows device status, IP address, and current connection
- **Network List**: Display all configured networks with status badges
- **Add Network**: Add new networks with auto-slot assignment
- **Delete Network**: Remove networks you no longer need (except primary)
- **Quick Connect**: One-click switching between networks
- **System Actions**: Reconnect, configure, or reboot device
- **Auto-refresh**: Updates every 15 seconds
- **Modern Design**: Responsive, mobile-friendly, gradient backgrounds

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

### Libraries

- **IotWebConf** (this library)
- **ArduinoJson** v6+
- **ESP8266WiFi** or **WiFi** (included with ESP core)
- **DNSServer** (included with ESP core)
- **WebServer** or **ESP8266WebServer** (included with ESP core)

## Usage

### 1. Initial Setup

1. Flash the example to your device
2. Device creates AP: **NetworkManager** with password `netmgr123`
3. Connect to the AP and configure your primary WiFi network
4. Device will connect to your WiFi and start the web interface

### 2. Web Interface

Open a browser and navigate to the device's IP address (shown in Serial Monitor):

```
http://192.168.1.100
```

The interface provides:
- Real-time device status
- List of all configured networks
- Quick connect buttons
- Credential update form
- System control buttons

### 3. API Usage Examples

#### List Configured Networks

```bash
curl http://192.168.1.100/api/networks/configured
```

Response:
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
      "id": "wifi2",
      "ssid": "OfficeNetwork",
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

#### Add Network (Phase 3 - Auto-Slot) ⭐ NEW

```bash
curl -X POST http://192.168.1.100/api/networks \
  -H "Content-Type: application/json" \
  -d '{"ssid":"CoffeeShopWiFi","password":"coffee123"}'
```

Response:
```json
{
  "status": "connecting",
  "id": "wifi2",
  "slot": 1,
  "ssid": "CoffeeShopWiFi"
}
```

The API automatically finds the next available slot (wifi2, wifi3, etc.) and assigns the network there.

#### Delete Network (Phase 3) ⭐ NEW

```bash
curl -X DELETE http://192.168.1.100/api/networks/wifi2
```

Response:
```json
{
  "status": "deleted",
  "id": "wifi2"
}
```

**Note:** Cannot delete the primary network (iwcWifi0).

#### Connect to a Network

```bash
curl -X POST http://192.168.1.100/api/networks/wifi2/connect
```

Response:
```json
{
  "status": "connecting",
  "id": "wifi2",
  "ssid": "OfficeNetwork"
}
```

#### Force Reconnection

```bash
curl -X POST http://192.168.1.100/api/reconnect
```

Response:
```json
{
  "status": "reconnecting"
}
```

### 4. Multiple WiFi Configuration

To enable multiple WiFi support:

1. Uncomment the **MULTIPLE WIFI CONFIGURATION** section in the code
2. Configure additional networks via the web UI at `/config`
3. Or update them via API using network IDs: `wifi2`, `wifi3`
4. Device will automatically try alternate networks on connection failure

## Code Structure

### Key Components

```cpp
// API instance with multiple WiFi support
IotWebConfApi api(&iotWebConf, &server);
api.setMultipleWifiAddition(&multiWifi);

// Register all API routes (includes Phase 3 endpoints)
api.registerRoutes();

// Main loop
void loop() {
  iotWebConf.doLoop();
  api.loop(); // Required for async WiFi scanning
}
```

### Phase 3 Workflow

**Adding a Network:**
1. POST /api/networks with {"ssid":"...", "password":"..."}
2. API finds next available slot automatically
3. Credentials saved to EEPROM
4. Device attempts connection
5. Returns assigned slot number

**Deleting a Network:**
1. DELETE /api/networks/{id}
2. Credentials cleared from slot (SSID and password emptied)
3. Changes saved to EEPROM
4. Slot becomes available for future networks

### Network Identification

Networks are identified by their parameter group IDs:
- **iwcWifi0** - Primary WiFi (always present, cannot be deleted)
- **wifi2**, **wifi3**, etc. - Additional networks (if MultipleWifiAddition is used)
- Custom IDs can be specified when creating ChainedWifiParameterGroup

## Customization

### Add More Networks

```cpp
char wifi4Ssid[IOTWEBCONF_WORD_LEN];
char wifi4Pass[IOTWEBCONF_PASSWORD_LEN];
ChainedWifiParameterGroup wifi4Group("Fourth WiFi", "wifi4", wifi4Ssid, wifi4Pass);

ChainedWifiParameterGroup* wifiChain[] = { &wifi2Group, &wifi3Group, &wifi4Group };
```

### Custom Web UI

The HTML is embedded in the `HTML_PAGE` constant. You can:
- Modify the styling (CSS in `<style>` block)
- Add new features (JavaScript in `<script>` block)
- Integrate with your own backend

### Custom API Endpoints

```cpp
// Add your own endpoints
server.on("/api/custom", []() {
  JsonDocument doc;
  doc["custom"] = "data";
  api.sendJson(200, doc); // Reuse sendJson helper
});
```

## Network ID Reference

| ID | Description | Source |
|----|-------------|--------|
| `iwcWifi0` | Primary WiFi network | IotWebConf default |
| `wifi2` | Second WiFi network | MultipleWifiAddition |
| `wifi3` | Third WiFi network | MultipleWifiAddition |
| Custom | Any ChainedWifiParameterGroup | User-defined |

The ID is specified when creating a ChainedWifiParameterGroup:
```cpp
ChainedWifiParameterGroup wifiGroup("Label", "networkId", ssid, password);
```

## Troubleshooting

### "Network not found" Error

If you get `"error": "Network not found"`:
- Check that the network ID matches exactly (case-sensitive)
- Ensure MultipleWifiAddition is enabled: `api.setMultipleWifiAddition(&multiWifi)`
- Verify the network slot exists in configured networks list
- Use GET /api/networks/configured to see all available network IDs

### "Cannot delete primary network" Error

The primary WiFi network (iwcWifi0) cannot be deleted via API:
- This is by design to prevent lockout
- Use the /config web page to modify primary credentials
- Or update via POST /api/config endpoint

### "No available network slots" Error

When adding networks, if all slots are full:
- Delete an unused network first with DELETE /api/networks/{id}
- Or add more slots by expanding the wifiChain array in code
- Check GET /api/networks/configured to see which slots are available

### EEPROM Write Warning

The API writes to EEPROM when adding/deleting/updating credentials. EEPROM has limited write cycles (~100,000):
- Avoid repeatedly calling these endpoints in loops
- Use POST /api/reconnect to reconnect without writing to EEPROM

### Connection Fails After Adding/Updating Network

- Verify credentials are correct
- Check WiFi network is in range
- Try POST /api/reconnect to retry connection
- Check Serial Monitor for error messages
- Try connecting via the web UI for better status feedback

### Web UI Not Loading

- Ensure device is connected to network (check Serial Monitor)
- Try accessing via mDNS: `http://networkmanager.local`
- Check firewall isn't blocking port 80
- Verify device IP address with GET /api/status

## Security Considerations

⚠️ **This example does NOT include authentication for API endpoints.**

In production, you should:
1. Add HTTP Basic Auth to all API endpoints
2. Use HTTPS (TLS) for encrypted communication
3. Implement rate limiting to prevent abuse
4. Validate all input thoroughly
5. Consider using API keys or tokens

Example with authentication:
```cpp
server.on("/api/networks/configured", [&]() {
  if (!server.authenticate("admin", "password")) {
    return server.requestAuthentication();
  }
  api.handleApiGetConfiguredNetworks();
});
```

## Related Examples

- **IotWebConf18JsonApi** - Basic JSON API usage
- **IotWebConf15MultipleWifi** - Multiple WiFi configuration
- **IotWebConf19AsyncServer** - AsyncWebServer integration

## Phase 3 API Documentation

For complete API documentation, see:
- `doc/JSON-API.md` - Full API reference
- `AgentProgress/009-API-Architecture-Design.md` - Design decisions
- `AgentProgress/018-Phase3-Complete.md` - Phase 3 implementation notes

## What's New in Phase 3

**Phase 2 → Phase 3 Changes:**

| Feature | Phase 2 | Phase 3 |
|---------|---------|---------|
| Add Network | POST /api/networks/{id} (manual slot) | POST /api/networks (auto-slot) ⭐ |
| Delete Network | Not available | DELETE /api/networks/{id} ⭐ |
| Slot Assignment | Manual | Automatic |
| Return Value | Status only | Status + slot + id |

**Migration Guide:**

Old (Phase 2):
```javascript
// Had to manually specify slot
POST /api/networks/wifi2
{"ssid": "test", "password": "pass"}
```

New (Phase 3):
```javascript
// Auto-assigns to next available slot
POST /api/networks
{"ssid": "test", "password": "pass"}

// Response tells you which slot was used
{
  "status": "connecting",
  "id": "wifi2",
  "slot": 1,
  "ssid": "test"
}
```

## License

MIT License - See LICENSE file in the repository root.

## Contributing

This is part of the IotWebConf library by Balazs Kelemen.  
Enhancements and bug reports: https://github.com/prampec/IotWebConf
