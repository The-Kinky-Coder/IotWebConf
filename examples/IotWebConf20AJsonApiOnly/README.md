# Example 20A: JSON API - Pure API Mode

**For developers building custom applications/UIs who want JSON-only network management**

## 🎯 What This Example Is For

This example is designed for **API-first developers** who want to manage WiFi networks programmatically via REST API without the traditional IotWebConf config portal workflow.

### ✅ Use This Example If You're Building:

- Mobile apps that manage device WiFi via REST API
- Custom web dashboards with your own UI
- Automated provisioning systems
- IoT management platforms
- Command-line tools for device configuration
- Any application where **JSON API is the primary interface**

### ❌ Don't Use This Example If:

- You want users to use BOTH the JSON API AND the traditional `/config` portal → See **Example 20B** instead
- You only need basic WiFi configuration → See **Example 01** or **Example 02**
- You need custom parameters beyond WiFi → See **Example 03** or adapt Example 20B

---

## 📋 What This Example Demonstrates

### Core Features

- ✅ **Pure JSON API usage** - No config portal integration needed
- ✅ **Dynamic network addition** - `POST /api/networks` with auto-slot assignment
- ✅ **Network deletion** - `DELETE /api/networks/{id}` to remove credentials
- ✅ **Network switching** - `POST /api/networks/{id}/connect` for quick switching
- ✅ **Status monitoring** - Real-time device and WiFi status
- ✅ **Modern web UI** - Complete single-page app for network management

### What This Does NOT Show

- ❌ Traditional IotWebConf config portal integration
- ❌ Manual WiFi credential entry via `/config` page
- ❌ Custom parameters (focuses on WiFi only)
- ❌ Hybrid API + config portal usage

---

## 🚀 Quick Start

### 1. Hardware Requirements

- ESP8266 or ESP32 development board
- USB cable for programming
- Optional: LED on `LED_BUILTIN` for status indication
- Optional: Button on GPIO 0 for configuration reset

### 2. Software Requirements

#### PlatformIO

Add to your `platformio.ini`:

```ini
[env:mydevice]
platform = espressif8266  ; or espressif32
board = nodemcuv2          ; or your board
framework = arduino

lib_deps = 
  prampec/IotWebConf
  bblanchon/ArduinoJson @ ^6.0.0

build_flags = 
  -D IOTWEBCONF_ENABLE_JSON
```

#### Arduino IDE

1. Install libraries via Library Manager:
   - **IotWebConf** by Balazs Kelemen
   - **ArduinoJson** v6+
2. The sketch automatically defines `IOTWEBCONF_ENABLE_JSON`

### 3. Upload and Configure

1. Flash the sketch to your device
2. Device creates AP: **ApiDevice** with password `apimode123`
3. Connect to the AP
4. Navigate to `192.168.4.1` or use the web UI
5. Add your primary WiFi network
6. Device connects and starts the JSON API server

### 4. Access the Web Interface

After connecting to your WiFi, find the device IP (shown in Serial Monitor) and open:

```
http://192.168.1.100
```

You'll see a modern, interactive network management interface.

---

## 📡 API Reference

### Core Endpoints

#### 1. Get Device Status

```bash
GET /api/status
```

**Response:**
```json
{
  "thingName": "ApiDevice",
  "version": "api1",
  "state": 4,
  "stateName": "Online",
  "apConnectionTimeout": 30000,
  "wifi": {
    "status": 3,
    "statusName": "Connected",
    "ssid": "MyHomeNetwork",
    "rssi": -45
  },
  "ip": "192.168.1.100",
  "apIp": "192.168.4.1"
}
```

#### 2. List Configured Networks

```bash
GET /api/networks/configured
```

**Response:**
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

#### 3. Add Network (Auto-Slot Assignment) ⭐

```bash
POST /api/networks
Content-Type: application/json

{
  "ssid": "CoffeeShopWiFi",
  "password": "coffee123"
}
```

**Response:**
```json
{
  "status": "connecting",
  "id": "wifi2",
  "slot": 1,
  "ssid": "CoffeeShopWiFi"
}
```

**How It Works:**
- API automatically finds the next available empty slot
- Saves credentials to that slot
- Attempts to connect to the network
- Returns the assigned slot ID for future reference

#### 4. Delete Network ⭐

```bash
DELETE /api/networks/wifi2
```

**Response:**
```json
{
  "status": "deleted",
  "id": "wifi2"
}
```

**Notes:**
- Cannot delete primary network (`iwcWifi0`)
- Clears SSID and password (sets to empty)
- Slot becomes available for future networks
- Changes saved to EEPROM

#### 5. Connect to Network

```bash
POST /api/networks/wifi2/connect
```

**Response:**
```json
{
  "status": "connecting",
  "id": "wifi2",
  "ssid": "OfficeNetwork"
}
```

**How It Works:**
- Copies network credentials to primary slot
- Triggers immediate WiFi reconnection
- Device switches to the specified network

#### 6. Force Reconnection

```bash
POST /api/reconnect
```

**Response:**
```json
{
  "status": "reconnecting"
}
```

**Use Cases:**
- Retry connection after temporary network issues
- Refresh connection without changing credentials
- Test connectivity

---

## 🔧 Understanding WiFi Slots

### What Are WiFi Slots?

WiFi slots are **storage locations** where network credentials are saved. Think of them as "presets" for different WiFi networks.

### Why Do I Need to Define Slots?

- Memory must be allocated at **compile time**
- Each slot reserves ~256 bytes of EEPROM/NVS storage
- More slots = more networks users can manage via API

### How Many Slots Do I Need?

| Use Case | Recommended Slots | Total Networks |
|----------|-------------------|----------------|
| Home only | 0-1 additional | 1-2 networks |
| Home + work | 2-3 additional | 3-4 networks |
| Multi-location | 4-5 additional | 5-6 networks |
| Enterprise | 8-10 additional | 9-11 networks |

**This example configures 5 additional slots (6 total including primary).**

### Slot Configuration

```cpp
#define NUM_WIFI_SLOTS 5  // Number of additional slots

iotwebconf::ChainedWifiParameterGroup wifiSlots[NUM_WIFI_SLOTS] = {
  {"wifi2"},  // Slot 1
  {"wifi3"},  // Slot 2
  {"wifi4"},  // Slot 3
  {"wifi5"},  // Slot 4
  {"wifi6"}   // Slot 5
};
```

### Slot IDs

| Slot ID | Index | Description |
|---------|-------|-------------|
| `iwcWifi0` | 0 | Primary network (always exists, cannot be deleted) |
| `wifi2` | 1 | First additional slot |
| `wifi3` | 2 | Second additional slot |
| `wifi4` | 3 | Third additional slot |
| `wifi5` | 4 | Fourth additional slot |
| `wifi6` | 5 | Fifth additional slot |

### How to Adjust Slot Count

**To add more slots:**

1. Increase `NUM_WIFI_SLOTS`:
   ```cpp
   #define NUM_WIFI_SLOTS 8  // More slots
   ```

2. Add more entries to the array:
   ```cpp
   iotwebconf::ChainedWifiParameterGroup wifiSlots[NUM_WIFI_SLOTS] = {
     {"wifi2"}, {"wifi3"}, {"wifi4"}, {"wifi5"}, 
     {"wifi6"}, {"wifi7"}, {"wifi8"}, {"wifi9"}
   };
   ```

**To reduce slots:**

1. Decrease `NUM_WIFI_SLOTS`:
   ```cpp
   #define NUM_WIFI_SLOTS 2  // Fewer slots
   ```

2. Remove entries from the array:
   ```cpp
   iotwebconf::ChainedWifiParameterGroup wifiSlots[NUM_WIFI_SLOTS] = {
     {"wifi2"}, {"wifi3"}
   };
   ```

---

## 💡 Usage Examples

### Example 1: Mobile App Provisioning

```javascript
// Mobile app connects to device AP
// Then provisions WiFi via API

async function provisionDevice(ssid, password) {
  // Add home network
  const response = await fetch('http://192.168.4.1/api/networks', {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({ ssid, password })
  });
  
  const result = await response.json();
  console.log(`Network added to slot ${result.id}`);
  
  // Device will auto-reconnect to the new network
  // App should now reconnect to home WiFi and find device there
}
```

### Example 2: Multi-Location Device

```bash
# Add networks for multiple locations
curl -X POST http://device.local/api/networks \
  -H "Content-Type: application/json" \
  -d '{"ssid":"HomeNetwork","password":"home123"}'

curl -X POST http://device.local/api/networks \
  -H "Content-Type: application/json" \
  -d '{"ssid":"OfficeNetwork","password":"office456"}'

curl -X POST http://device.local/api/networks \
  -H "Content-Type: application/json" \
  -d '{"ssid":"WarehouseNetwork","password":"warehouse789"}'

# Switch to office network when at work
curl -X POST http://device.local/api/networks/wifi3/connect
```

### Example 3: Automated Provisioning

```python
import requests
import json

def provision_fleet(devices, network_config):
    """Provision multiple devices with network credentials"""
    for device in devices:
        url = f"http://{device['ip']}/api/networks"
        
        for network in network_config:
            response = requests.post(
                url,
                headers={'Content-Type': 'application/json'},
                json={'ssid': network['ssid'], 'password': network['password']}
            )
            
            result = response.json()
            print(f"Device {device['name']}: Network added to {result['id']}")

# Usage
devices = [
    {'name': 'Sensor-01', 'ip': '192.168.1.100'},
    {'name': 'Sensor-02', 'ip': '192.168.1.101'},
    {'name': 'Sensor-03', 'ip': '192.168.1.102'}
]

networks = [
    {'ssid': 'PrimaryNetwork', 'password': 'primary123'},
    {'ssid': 'BackupNetwork', 'password': 'backup456'}
]

provision_fleet(devices, networks)
```

### Example 4: CLI Tool

```bash
#!/bin/bash
# Device WiFi management script

DEVICE_IP="192.168.1.100"

# Function to add network
add_network() {
    local ssid=$1
    local password=$2
    
    curl -X POST "http://${DEVICE_IP}/api/networks" \
        -H "Content-Type: application/json" \
        -d "{\"ssid\":\"${ssid}\",\"password\":\"${password}\"}"
}

# Function to list networks
list_networks() {
    curl -s "http://${DEVICE_IP}/api/networks/configured" | jq .
}

# Function to delete network
delete_network() {
    local slot_id=$1
    curl -X DELETE "http://${DEVICE_IP}/api/networks/${slot_id}"
}

# Function to connect to network
connect_network() {
    local slot_id=$1
    curl -X POST "http://${DEVICE_IP}/api/networks/${slot_id}/connect"
}

# Usage
case "$1" in
    add)
        add_network "$2" "$3"
        ;;
    list)
        list_networks
        ;;
    delete)
        delete_network "$2"
        ;;
    connect)
        connect_network "$2"
        ;;
    *)
        echo "Usage: $0 {add|list|delete|connect} [args]"
        exit 1
        ;;
esac
```

---

## 🎨 Web UI Features

The included web interface provides:

### Dashboard
- Real-time device status
- Current WiFi connection info
- IP address display
- Connection state indicator

### Network Management
- Visual list of all configured networks
- Status badges (Current, Active, Inactive)
- Quick connect buttons
- Delete functionality
- Empty slot indicators

### Add Network Form
- Simple SSID/password entry
- Support for open networks (no password)
- Immediate feedback on success/failure
- Shows assigned slot ID

### System Actions
- Reconnect WiFi button
- Reboot device button
- Auto-refresh every 15 seconds

### Design Features
- Modern gradient backgrounds
- Responsive mobile layout
- Smooth animations
- Clear status indicators
- Color-coded feedback messages

---

## 🔒 Security Considerations

⚠️ **This example does NOT include authentication for API endpoints.**

### Production Recommendations

1. **Add Authentication**
   ```cpp
   server.on("/api/networks", []() {
     if (!server.authenticate("admin", "password")) {
       return server.requestAuthentication();
     }
     api.handleApiAddNetwork();
   });
   ```

2. **Use HTTPS/TLS**
   - Encrypt all communication
   - Protect credentials in transit
   - See ESP32 WiFiClientSecure examples

3. **Implement Rate Limiting**
   - Prevent brute force attacks
   - Limit API calls per minute
   - Track failed attempts

4. **Validate All Input**
   - Check SSID length and characters
   - Validate password requirements
   - Sanitize all user input

5. **Use API Keys/Tokens**
   ```cpp
   const char* API_KEY = "your-secret-key";
   
   if (server.header("X-API-Key") != API_KEY) {
     server.send(401, "application/json", "{\"error\":\"Unauthorized\"}");
     return;
   }
   ```

6. **Implement CORS Carefully**
   - Only allow trusted origins
   - Don't use `Access-Control-Allow-Origin: *` in production

7. **Log Security Events**
   - Track failed authentication
   - Monitor unusual activity
   - Alert on suspicious patterns

---

## 🐛 Troubleshooting

### "Network not found" Error

**Problem:** API returns `{"error": "Network not found"}`

**Solutions:**
- Check that network ID matches exactly (case-sensitive)
- Use `GET /api/networks/configured` to see available IDs
- Ensure MultipleWifiAddition is enabled in code
- Verify slot was created in the wifiSlots array

### "No available network slots" Error

**Problem:** Cannot add more networks

**Solutions:**
- Delete unused networks: `DELETE /api/networks/{id}`
- Increase `NUM_WIFI_SLOTS` in the sketch
- Check how many slots are configured vs. used

### "Cannot delete primary network" Error

**Problem:** Trying to delete `iwcWifi0`

**Solutions:**
- This is by design to prevent lockout
- Update primary network via web UI or connect to different network first
- Use `POST /api/networks/{id}/connect` to switch primary

### Connection Fails After Adding Network

**Symptoms:** Network added but device doesn't connect

**Solutions:**
1. Verify credentials are correct
2. Check WiFi network is in range
3. Try `POST /api/reconnect` to retry
4. Check Serial Monitor for error messages
5. Verify network supports 2.4GHz (ESP8266/ESP32 don't support 5GHz)

### Web UI Not Loading

**Symptoms:** Cannot access web interface

**Solutions:**
1. Check device is connected (LED status)
2. Verify IP address in Serial Monitor
3. Try accessing via AP mode: `192.168.4.1`
4. Check firewall isn't blocking port 80
5. Try mDNS: `http://apidevice.local` (may not work on all networks)

### API Returns HTML Instead of JSON

**Symptoms:** Getting HTML response from API endpoints

**Solutions:**
1. Ensure you're accessing the correct endpoint
2. Check for typos in URL
3. Verify `IOTWEBCONF_ENABLE_JSON` is defined
4. Ensure ArduinoJson library is installed
5. Check Serial Monitor for initialization errors

### EEPROM Write Warnings

**Symptoms:** Rapid EEPROM writes, potential wear

**Solutions:**
- Don't call add/delete/update endpoints in loops
- Use `POST /api/reconnect` for connection retries (doesn't write EEPROM)
- Batch multiple configuration changes together
- EEPROM typically supports ~100,000 write cycles

---

## 📊 API Workflow Examples

### First-Time Setup Flow

```
1. Device starts in AP mode (no WiFi configured)
   └─→ SSID: "ApiDevice", Password: "apimode123"

2. User connects to AP and calls:
   POST /api/networks
   {"ssid": "HomeNetwork", "password": "home123"}

3. Device saves credentials and connects
   └─→ Returns: {"status": "connecting", "id": "iwcWifi0", "slot": 0}

4. Device obtains IP on home network
   └─→ Now accessible at http://192.168.1.100

5. User finds device on home network and adds more networks:
   POST /api/networks
   {"ssid": "OfficeNetwork", "password": "office456"}
   └─→ Returns: {"status": "saved", "id": "wifi2", "slot": 1}
```

### Network Switching Flow

```
1. List available networks:
   GET /api/networks/configured
   └─→ Returns: {"networks": [{"id": "iwcWifi0", ...}, {"id": "wifi2", ...}]}

2. Connect to office network:
   POST /api/networks/wifi2/connect
   └─→ Device switches to office WiFi

3. Verify connection:
   GET /api/status
   └─→ Returns: {"wifi": {"ssid": "OfficeNetwork", "status": 3}}
```

### Network Cleanup Flow

```
1. List all networks:
   GET /api/networks/configured

2. Delete unused network:
   DELETE /api/networks/wifi3
   └─→ Slot becomes available for new networks

3. Add replacement network:
   POST /api/networks
   {"ssid": "NewNetwork", "password": "new123"}
   └─→ Uses previously freed slot
```

---

## 🔗 Related Examples

| Example | Purpose | When to Use |
|---------|---------|-------------|
| **IotWebConf01Minimal** | Basic WiFi setup | Simple projects, learning |
| **IotWebConf15MultipleWifi** | Multiple WiFi without API | Fallback WiFi, no API needed |
| **IotWebConf18JsonApi** | Basic JSON API | Getting started with API |
| **IotWebConf20NetworkManagement** | Original full example | Reference implementation |
| **IotWebConf20B (coming)** | Hybrid API + Config Portal | Mixed usage scenarios |

---

## 📚 Additional Documentation

- **JSON API Reference:** `doc/JSON-API.md`
- **Multiple WiFi Guide:** `doc/MultipleWifi.md`
- **Architecture Design:** `AgentProgress/009-API-Architecture-Design.md`
- **Phase 3 Implementation:** `AgentProgress/018-Phase3-Complete.md`

---

## 🤝 Contributing

This is part of the IotWebConf library by Balazs Kelemen.

- **Issues & Features:** https://github.com/prampec/IotWebConf/issues
- **Discussions:** https://github.com/prampec/IotWebConf/discussions
- **Documentation:** https://github.com/prampec/IotWebConf/wiki

---

## 📄 License

MIT License - See LICENSE file in the repository root.

---

## 🎓 Learning Path

### If You're New to IotWebConf:

1. Start with **Example 01** (Minimal) to understand basics
2. Try **Example 02** (StatusAndReset) for real-world usage
3. Move to **Example 18** (JsonApi) for basic API concepts
4. Use **This Example (20A)** for pure API development

### If You're New to JSON APIs:

1. Test endpoints with `curl` first (examples above)
2. Use the web UI to understand the workflow
3. Check Serial Monitor to see what's happening
4. Read the API responses carefully - they contain helpful info

### Next Steps:

- Add authentication to secure your API
- Integrate with your mobile/web application
- Build custom provisioning workflows
- Explore other IotWebConf features (OTA updates, custom parameters, etc.)

---

**Happy Coding! 🚀**

If you have questions or find issues, please open a GitHub issue or discussion.
