/**
 * IotWebConf20NetworkManagement.ino -- IotWebConf is an ESP8266/ESP32
 *   non blocking WiFi/AP web configuration library for Arduino.
 *   https://github.com/prampec/IotWebConf
 *
 * Copyright (C) 2020 Balazs Kelemen <prampec+arduino@gmail.com>
 *
 * This software may be modified and distributed under the terms
 * of the MIT license.  See the LICENSE file for details.
 */

/**
 * Example: Network Management JSON API
 * 
 * This example demonstrates the Phase 2 network management API endpoints:
 * 
 * 1. GET  /api/networks/configured  - List all configured WiFi networks
 * 2. POST /api/networks/{id}         - Update credentials for a network
 * 3. POST /api/networks/{id}/connect - Connect to a specific network
 * 4. POST /api/reconnect             - Force WiFi reconnection
 * 
 * This example also shows how to:
 * - Use the JSON API with multiple WiFi networks
 * - Integrate custom web UI with IotWebConf
 * - Handle network switching programmatically
 * 
 * Requirements:
 * - Build flag: -D IOTWEBCONF_ENABLE_JSON (automatically set below for Arduino IDE)
 * - Library: ArduinoJson v6+
 * - Optional: MultipleWifi support (shown below)
 * 
 * Test with curl:
 * 
 *   # List configured networks
 *   curl http://192.168.1.100/api/networks/configured
 * 
 *   # Update network credentials
 *   curl -X POST http://192.168.1.100/api/networks/iwcWifi0 \
 *     -H "Content-Type: application/json" \
 *     -d '{"ssid":"MyNetwork","password":"MyPassword"}'
 * 
 *   # Connect to a specific network
 *   curl -X POST http://192.168.1.100/api/networks/wifi1/connect
 * 
 *   # Force reconnection
 *   curl -X POST http://192.168.1.100/api/reconnect
 * 
 * Web UI:
 *   Open http://192.168.1.100/ for an interactive network management interface
 */

// ═══════════════════════════════════════════════════════
// REQUIRED: Enable JSON API support
// ═══════════════════════════════════════════════════════
#ifndef IOTWEBCONF_ENABLE_JSON
#define IOTWEBCONF_ENABLE_JSON
#endif

#include <IotWebConf.h>
#include <IotWebConfUsing.h>
#include <IotWebConfApi.h>
#include <IotWebConfMultipleWifi.h> // For multiple WiFi support

// -- Initial name, password, and configuration version
const char thingName[] = "NetworkManager";
const char wifiInitialApPassword[] = "netmgr123";

#define STRING_LEN 128
#define CONFIG_VERSION "nm1"

// -- Status indicator pin
// Note: LED_BUILTIN may not be defined on all boards (e.g., ESP32-S3-USB-OTG)
// If compilation fails, replace with a valid GPIO number or -1 to disable
#ifndef LED_BUILTIN
#define LED_BUILTIN 2  // Default to GPIO 2 if LED_BUILTIN not defined
#endif
#define STATUS_PIN LED_BUILTIN

// -- Configuration reset button pin
#define CONFIG_PIN 0

// -- Web server and DNS
DNSServer dnsServer;
WebServer server(80);

// -- IotWebConf instance
IotWebConf iotWebConf(thingName, &dnsServer, &server, wifiInitialApPassword, CONFIG_VERSION);

// -- JSON API instance
IotWebConfApi api(&iotWebConf, &server);

// ═══════════════════════════════════════════════════════
// MULTIPLE WIFI CONFIGURATION (OPTIONAL)
// ═══════════════════════════════════════════════════════
// Uncomment this section to enable multiple WiFi network support

// WiFi credentials for additional networks
char wifi2Ssid[IOTWEBCONF_WORD_LEN];
char wifi2Pass[IOTWEBCONF_PASSWORD_LEN];
ChainedWifiParameterGroup wifi2Group("Connect to WiFi", "wifi2", wifi2Ssid, wifi2Pass);

char wifi3Ssid[IOTWEBCONF_WORD_LEN];
char wifi3Pass[IOTWEBCONF_PASSWORD_LEN];
ChainedWifiParameterGroup wifi3Group("Alternative WiFi", "wifi3", wifi3Ssid, wifi3Pass);

// Chain the WiFi parameter groups
ChainedWifiParameterGroup* wifiChain[] = { &wifi2Group, &wifi3Group };

// Multiple WiFi addition
MultipleWifiAddition multiWifi(
  wifiChain,
  sizeof(wifiChain) / sizeof(wifiChain[0]),
  []() { iotWebConf.saveConfig(); }
);

// ═══════════════════════════════════════════════════════

// -- Custom parameter example
char customValue[STRING_LEN];
IotWebConfTextParameter customParam = IotWebConfTextParameter(
  "Custom Parameter", "customParam", customValue, STRING_LEN
);

// ═══════════════════════════════════════════════════════
// HTML WEB UI FOR NETWORK MANAGEMENT
// ═══════════════════════════════════════════════════════

const char HTML_PAGE[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>Network Manager</title>
  <style>
    body {
      font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, 'Helvetica Neue', Arial, sans-serif;
      max-width: 800px;
      margin: 40px auto;
      padding: 20px;
      background: #f5f5f5;
    }
    .card {
      background: white;
      border-radius: 8px;
      box-shadow: 0 2px 4px rgba(0,0,0,0.1);
      padding: 20px;
      margin-bottom: 20px;
    }
    h1 { margin-top: 0; color: #333; }
    h2 { color: #666; border-bottom: 2px solid #007bff; padding-bottom: 10px; }
    .network {
      border: 1px solid #ddd;
      border-radius: 4px;
      padding: 15px;
      margin-bottom: 10px;
      background: #fafafa;
    }
    .network.current {
      border-color: #28a745;
      background: #e8f5e9;
    }
    .network-header {
      display: flex;
      justify-content: space-between;
      align-items: center;
      margin-bottom: 10px;
    }
    .badge {
      padding: 4px 8px;
      border-radius: 4px;
      font-size: 12px;
      font-weight: bold;
    }
    .badge-success { background: #28a745; color: white; }
    .badge-info { background: #17a2b8; color: white; }
    .badge-warning { background: #ffc107; color: black; }
    button {
      background: #007bff;
      color: white;
      border: none;
      padding: 8px 16px;
      border-radius: 4px;
      cursor: pointer;
      margin-right: 5px;
    }
    button:hover { background: #0056b3; }
    button.danger { background: #dc3545; }
    button.danger:hover { background: #c82333; }
    button.success { background: #28a745; }
    button.success:hover { background: #218838; }
    input[type="text"], input[type="password"] {
      width: 100%;
      padding: 8px;
      margin: 5px 0;
      border: 1px solid #ddd;
      border-radius: 4px;
      box-sizing: border-box;
    }
    .form-group { margin-bottom: 15px; }
    label { display: block; margin-bottom: 5px; color: #666; }
    .info { background: #e7f3ff; padding: 10px; border-radius: 4px; margin-bottom: 15px; }
    .status { margin-top: 10px; padding: 10px; border-radius: 4px; }
    .status.success { background: #d4edda; color: #155724; }
    .status.error { background: #f8d7da; color: #721c24; }
    .hidden { display: none; }
  </style>
</head>
<body>
  <div class="card">
    <h1>🌐 Network Manager</h1>
    <div class="info">
      <strong>Device:</strong> <span id="deviceName">Loading...</span><br>
      <strong>IP Address:</strong> <span id="ipAddress">Loading...</span><br>
      <strong>Status:</strong> <span id="wifiStatus">Loading...</span><br>
      <strong>Current SSID:</strong> <span id="currentSsid">Loading...</span>
    </div>
  </div>

  <div class="card">
    <h2>📡 Configured Networks</h2>
    <div id="networkList">Loading networks...</div>
    <div id="status" class="status hidden"></div>
  </div>

  <div class="card">
    <h2>➕ Update Network</h2>
    <div class="form-group">
      <label>Network ID:</label>
      <input type="text" id="updateNetworkId" placeholder="e.g., iwcWifi0, wifi2">
    </div>
    <div class="form-group">
      <label>SSID:</label>
      <input type="text" id="updateSsid" placeholder="Network name">
    </div>
    <div class="form-group">
      <label>Password:</label>
      <input type="password" id="updatePassword" placeholder="Network password">
    </div>
    <button onclick="updateNetwork()">Update Credentials</button>
    <div id="updateStatus" class="status hidden"></div>
  </div>

  <div class="card">
    <h2>⚙️ System Actions</h2>
    <button class="success" onclick="reconnect()">🔄 Reconnect WiFi</button>
    <button onclick="location.href='/config'">⚙️ Configure Device</button>
    <button class="danger" onclick="if(confirm('Reboot device?')) fetch('/api/reboot', {method:'POST'})">🔴 Reboot</button>
  </div>

  <script>
    let networks = [];

    // Load device status
    async function loadStatus() {
      try {
        const res = await fetch('/api/status');
        const data = await res.json();
        document.getElementById('deviceName').textContent = data.thingName || 'Unknown';
        document.getElementById('ipAddress').textContent = data.ip || 'Unknown';
        document.getElementById('wifiStatus').textContent = data.wifi.status === 3 ? '✅ Connected' : '❌ Disconnected';
        document.getElementById('currentSsid').textContent = data.wifi.ssid || 'None';
      } catch (e) {
        console.error('Failed to load status:', e);
      }
    }

    // Load configured networks
    async function loadNetworks() {
      try {
        const res = await fetch('/api/networks/configured');
        const data = await res.json();
        networks = data.networks || [];
        renderNetworks();
      } catch (e) {
        document.getElementById('networkList').innerHTML = '<p style="color:red;">Failed to load networks</p>';
      }
    }

    // Render network list
    function renderNetworks() {
      const container = document.getElementById('networkList');
      if (networks.length === 0) {
        container.innerHTML = '<p>No networks configured</p>';
        return;
      }

      container.innerHTML = networks.map(net => `
        <div class="network ${net.isCurrent ? 'current' : ''}">
          <div class="network-header">
            <div>
              <strong>${net.ssid || '(empty)'}</strong>
              ${net.isCurrent ? '<span class="badge badge-success">CURRENT</span>' : ''}
              ${net.isActive ? '<span class="badge badge-info">ACTIVE</span>' : '<span class="badge badge-warning">INACTIVE</span>'}
              ${net.hasPassword ? '🔒' : '🔓'}
            </div>
            <div>
              <small style="color:#999;">ID: ${net.id}</small>
            </div>
          </div>
          <button onclick="connectToNetwork('${net.id}')" ${!net.isActive ? 'disabled' : ''}>
            ${net.isCurrent ? '✓ Connected' : '📡 Connect'}
          </button>
          <button onclick="fillUpdateForm('${net.id}', '${net.ssid}')">✏️ Edit</button>
        </div>
      `).join('');
    }

    // Connect to a network
    async function connectToNetwork(id) {
      showStatus('Connecting to network...', 'info');
      try {
        const res = await fetch(`/api/networks/${id}/connect`, { method: 'POST' });
        const data = await res.json();
        if (res.ok) {
          showStatus(`✅ Connecting to ${data.ssid}...`, 'success');
          setTimeout(() => {
            loadStatus();
            loadNetworks();
          }, 3000);
        } else {
          showStatus(`❌ Error: ${data.error}`, 'error');
        }
      } catch (e) {
        showStatus('❌ Connection failed', 'error');
      }
    }

    // Update network credentials
    async function updateNetwork() {
      const id = document.getElementById('updateNetworkId').value.trim();
      const ssid = document.getElementById('updateSsid').value.trim();
      const password = document.getElementById('updatePassword').value;

      if (!id || !ssid) {
        showUpdateStatus('❌ Please fill in Network ID and SSID', 'error');
        return;
      }

      showUpdateStatus('Updating network...', 'info');
      try {
        const res = await fetch(`/api/networks/${id}`, {
          method: 'POST',
          headers: { 'Content-Type': 'application/json' },
          body: JSON.stringify({ ssid, password })
        });
        const data = await res.json();
        if (res.ok) {
          showUpdateStatus(`✅ Network ${id} updated successfully`, 'success');
          loadNetworks();
          // Clear form
          document.getElementById('updateNetworkId').value = '';
          document.getElementById('updateSsid').value = '';
          document.getElementById('updatePassword').value = '';
        } else {
          showUpdateStatus(`❌ Error: ${data.error}`, 'error');
        }
      } catch (e) {
        showUpdateStatus('❌ Update failed', 'error');
      }
    }

    // Reconnect WiFi
    async function reconnect() {
      showStatus('Reconnecting WiFi...', 'info');
      try {
        const res = await fetch('/api/reconnect', { method: 'POST' });
        const data = await res.json();
        if (res.ok) {
          showStatus('✅ WiFi reconnecting...', 'success');
          setTimeout(() => {
            loadStatus();
            loadNetworks();
          }, 3000);
        }
      } catch (e) {
        showStatus('❌ Reconnect failed', 'error');
      }
    }

    // Fill update form with network data
    function fillUpdateForm(id, ssid) {
      document.getElementById('updateNetworkId').value = id;
      document.getElementById('updateSsid').value = ssid;
      document.getElementById('updatePassword').value = '';
      document.getElementById('updatePassword').focus();
    }

    // Show status message
    function showStatus(message, type) {
      const el = document.getElementById('status');
      el.textContent = message;
      el.className = `status ${type}`;
      el.classList.remove('hidden');
      setTimeout(() => el.classList.add('hidden'), 5000);
    }

    function showUpdateStatus(message, type) {
      const el = document.getElementById('updateStatus');
      el.textContent = message;
      el.className = `status ${type}`;
      el.classList.remove('hidden');
      setTimeout(() => el.classList.add('hidden'), 5000);
    }

    // Initial load
    loadStatus();
    loadNetworks();
    
    // Refresh every 10 seconds
    setInterval(() => {
      loadStatus();
      loadNetworks();
    }, 10000);
  </script>
</body>
</html>
)rawliteral";

void handleRoot()
{
  // Check for captive portal
  if (iotWebConf.handleCaptivePortal())
  {
    return;
  }

  // Serve the HTML page
  server.send(200, "text/html", HTML_PAGE);
}

void setup() 
{
  Serial.begin(115200);
  Serial.println();
  Serial.println("Starting IotWebConf 20 - Network Management API...");

  // -- Set up IotWebConf
  iotWebConf.setStatusPin(STATUS_PIN);
  iotWebConf.setConfigPin(CONFIG_PIN);
  
  // -- Add custom parameter
  iotWebConf.addSystemParameter(&customParam);

  // ═══════════════════════════════════════════════════════
  // OPTIONAL: Enable multiple WiFi support
  // ═══════════════════════════════════════════════════════
  // Uncomment to enable multiple WiFi networks
  
  iotWebConf.addParameterGroup(&multiWifi);
  api.setMultipleWifiAddition(&multiWifi);

  // ═══════════════════════════════════════════════════════

  // -- Initialize IotWebConf
  iotWebConf.init();

  // -- Set up web server routes
  server.on("/", handleRoot);
  server.on("/config", []{ iotWebConf.handleConfig(); });
  server.onNotFound([]{ iotWebConf.handleNotFound(); });
  
  // -- Register all JSON API routes
  api.registerRoutes();

  Serial.println("Ready!");
  Serial.print("Access the web interface at: http://");
  Serial.println(WiFi.localIP());
  Serial.println();
  Serial.println("Available API endpoints:");
  Serial.println("  GET  /api/status");
  Serial.println("  GET  /api/networks/configured");
  Serial.println("  POST /api/networks/{id}");
  Serial.println("  POST /api/networks/{id}/connect");
  Serial.println("  POST /api/reconnect");
  Serial.println("  GET  /api/config");
  Serial.println("  POST /api/config");
  Serial.println("  GET  /api/networks");
  Serial.println("  POST /api/scan");
}

void loop() 
{
  // -- IotWebConf main loop
  iotWebConf.doLoop();
  
  // -- API background tasks (handles async WiFi scans)
  api.loop();
}
