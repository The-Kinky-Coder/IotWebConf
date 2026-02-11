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
 * This example demonstrates the Phase 3 network management API endpoints:
 * 
 * 1. GET    /api/networks/configured  - List all configured WiFi networks
 * 2. POST   /api/networks             - Add network (auto-assigns to next available slot)
 * 3. DELETE /api/networks/{id}        - Delete a network's credentials
 * 4. POST   /api/networks/{id}/connect - Connect to a specific network
 * 5. POST   /api/reconnect            - Force WiFi reconnection
 * 
 * This example also shows how to:
 * - Use the JSON API with multiple WiFi networks
 * - Integrate custom web UI with IotWebConf
 * - Handle network switching programmatically
 * - Add and remove networks dynamically
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
 *   # Add new network (auto-assigns slot)
 *   curl -X POST http://192.168.1.100/api/networks \
 *     -H "Content-Type: application/json" \
 *     -d '{"ssid":"MyNetwork","password":"MyPassword"}'
 * 
 *   # Delete network credentials
 *   curl -X DELETE http://192.168.1.100/api/networks/wifi2
 * 
 *   # Connect to a specific network
 *   curl -X POST http://192.168.1.100/api/networks/wifi2/connect
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
iotwebconf::ChainedWifiParameterGroup wifi2Group("wifi2");
iotwebconf::ChainedWifiParameterGroup wifi3Group("wifi3");

// Chain the WiFi parameter groups
iotwebconf::ChainedWifiParameterGroup wifiChain[] = { wifi2Group, wifi3Group };

// Multiple WiFi addition
iotwebconf::MultipleWifiAddition multiWifi(
  &iotWebConf,
  wifiChain,
  sizeof(wifiChain) / sizeof(wifiChain[0])
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
<html lang="en">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>Network Manager</title>
  <style>
    * { box-sizing: border-box; }
    body {
      font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, 'Helvetica Neue', Arial, sans-serif;
      max-width: 900px;
      margin: 0 auto;
      padding: 20px;
      background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
      min-height: 100vh;
    }
    .card {
      background: white;
      border-radius: 12px;
      box-shadow: 0 4px 6px rgba(0,0,0,0.1);
      padding: 24px;
      margin-bottom: 20px;
      animation: slideIn 0.3s ease-out;
    }
    @keyframes slideIn {
      from { opacity: 0; transform: translateY(20px); }
      to { opacity: 1; transform: translateY(0); }
    }
    h1 { 
      margin: 0 0 20px 0; 
      color: #1a1a1a; 
      font-size: 28px;
      display: flex;
      align-items: center;
      gap: 10px;
    }
    h2 { 
      color: #333; 
      border-bottom: 3px solid #667eea; 
      padding-bottom: 10px; 
      margin-top: 0;
      font-size: 20px;
    }
    .info {
      background: linear-gradient(135deg, #e0e7ff 0%, #f0e7ff 100%);
      padding: 16px;
      border-radius: 8px;
      margin-bottom: 0;
      border-left: 4px solid #667eea;
    }
    .info-line {
      display: flex;
      padding: 4px 0;
    }
    .info-line strong {
      min-width: 140px;
      color: #555;
    }
    .info-line span {
      color: #1a1a1a;
      font-weight: 500;
    }
    .network {
      border: 2px solid #e5e7eb;
      border-radius: 8px;
      padding: 16px;
      margin-bottom: 12px;
      background: #fafafa;
      transition: all 0.2s ease;
    }
    .network:hover {
      border-color: #667eea;
      box-shadow: 0 2px 8px rgba(102,126,234,0.2);
    }
    .network.current {
      border-color: #10b981;
      background: linear-gradient(135deg, #d1fae5 0%, #a7f3d0 100%);
    }
    .network-header {
      display: flex;
      justify-content: space-between;
      align-items: flex-start;
      margin-bottom: 12px;
      gap: 10px;
    }
    .network-title {
      flex: 1;
    }
    .network-ssid {
      font-size: 18px;
      font-weight: 600;
      color: #1a1a1a;
      margin-bottom: 8px;
      word-break: break-word;
    }
    .network-badges {
      display: flex;
      gap: 6px;
      flex-wrap: wrap;
    }
    .badge {
      padding: 4px 10px;
      border-radius: 12px;
      font-size: 11px;
      font-weight: 700;
      text-transform: uppercase;
      letter-spacing: 0.5px;
    }
    .badge-success { background: #10b981; color: white; }
    .badge-info { background: #3b82f6; color: white; }
    .badge-warning { background: #f59e0b; color: white; }
    .network-id {
      font-size: 12px;
      color: #6b7280;
      font-family: 'Courier New', monospace;
    }
    .network-actions {
      display: flex;
      gap: 8px;
      flex-wrap: wrap;
    }
    button {
      background: #667eea;
      color: white;
      border: none;
      padding: 10px 18px;
      border-radius: 6px;
      cursor: pointer;
      font-size: 14px;
      font-weight: 600;
      transition: all 0.2s ease;
      display: inline-flex;
      align-items: center;
      gap: 6px;
    }
    button:hover {
      background: #5568d3;
      transform: translateY(-1px);
      box-shadow: 0 4px 8px rgba(102,126,234,0.3);
    }
    button:active {
      transform: translateY(0);
    }
    button:disabled {
      background: #d1d5db;
      cursor: not-allowed;
      transform: none;
      box-shadow: none;
    }
    button.danger { background: #ef4444; }
    button.danger:hover { background: #dc2626; }
    button.success { background: #10b981; }
    button.success:hover { background: #059669; }
    button.warning { background: #f59e0b; }
    button.warning:hover { background: #d97706; }
    button.secondary { background: #6b7280; }
    button.secondary:hover { background: #4b5563; }
    input[type="text"], input[type="password"] {
      width: 100%;
      padding: 12px;
      margin: 5px 0;
      border: 2px solid #e5e7eb;
      border-radius: 6px;
      font-size: 14px;
      transition: border-color 0.2s;
    }
    input[type="text"]:focus, input[type="password"]:focus {
      outline: none;
      border-color: #667eea;
      box-shadow: 0 0 0 3px rgba(102,126,234,0.1);
    }
    .form-group { margin-bottom: 16px; }
    label { 
      display: block; 
      margin-bottom: 6px; 
      color: #374151;
      font-weight: 600;
      font-size: 14px;
    }
    .message {
      margin-top: 12px;
      padding: 12px 16px;
      border-radius: 6px;
      font-weight: 500;
      animation: slideIn 0.3s ease-out;
    }
    .message.success { 
      background: #d1fae5; 
      color: #065f46; 
      border-left: 4px solid #10b981;
    }
    .message.error { 
      background: #fee2e2; 
      color: #991b1b; 
      border-left: 4px solid #ef4444;
    }
    .message.info { 
      background: #dbeafe; 
      color: #1e40af; 
      border-left: 4px solid #3b82f6;
    }
    .hidden { display: none !important; }
    .empty-state {
      text-align: center;
      padding: 40px 20px;
      color: #6b7280;
    }
    .empty-state-icon {
      font-size: 48px;
      margin-bottom: 16px;
    }
    .system-actions {
      display: flex;
      gap: 10px;
      flex-wrap: wrap;
    }
    @media (max-width: 600px) {
      body { padding: 12px; }
      .card { padding: 16px; }
      h1 { font-size: 24px; }
      .network-header { flex-direction: column; }
      .network-actions { width: 100%; }
      .network-actions button { flex: 1; }
      .system-actions { flex-direction: column; }
      .system-actions button { width: 100%; }
    }
  </style>
</head>
<body>
  <div class="card">
    <h1><span>🌐</span> Network Manager</h1>
    <div class="info">
      <div class="info-line"><strong>Device:</strong> <span id="deviceName">Loading...</span></div>
      <div class="info-line"><strong>IP Address:</strong> <span id="ipAddress">Loading...</span></div>
      <div class="info-line"><strong>Status:</strong> <span id="wifiStatus">Loading...</span></div>
      <div class="info-line"><strong>Current SSID:</strong> <span id="currentSsid">Loading...</span></div>
    </div>
  </div>

  <div class="card">
    <h2>📡 Configured Networks</h2>
    <div id="networkList">
      <div class="empty-state">
        <div class="empty-state-icon">⏳</div>
        <div>Loading networks...</div>
      </div>
    </div>
    <div id="networkMessage" class="message hidden"></div>
  </div>

  <div class="card">
    <h2>➕ Add New Network</h2>
    <form id="addForm" onsubmit="addNetwork(event)">
      <div class="form-group">
        <label for="addSsid">Network Name (SSID) *</label>
        <input type="text" id="addSsid" placeholder="Enter WiFi network name" required>
      </div>
      <div class="form-group">
        <label for="addPassword">Password (leave empty for open networks)</label>
        <input type="password" id="addPassword" placeholder="Enter WiFi password">
      </div>
      <button type="submit">➕ Add Network</button>
    </form>
    <div id="addMessage" class="message hidden"></div>
  </div>

  <div class="card">
    <h2>⚙️ System Actions</h2>
    <div class="system-actions">
      <button class="success" onclick="reconnect()">🔄 Reconnect WiFi</button>
      <button class="secondary" onclick="location.href='/config'">⚙️ Configure Device</button>
      <button class="danger" onclick="rebootDevice()">🔴 Reboot Device</button>
    </div>
  </div>

  <script>
    let networks = [];
    let refreshInterval = null;

    // Load device status
    async function loadStatus() {
      try {
        const res = await fetch('/api/status');
        if (!res.ok) throw new Error('Status request failed');
        const data = await res.json();
        
        document.getElementById('deviceName').textContent = data.thingName || 'Unknown';
        document.getElementById('ipAddress').textContent = data.ip || 'Unknown';
        
        const isConnected = data.wifi.status === 3;
        document.getElementById('wifiStatus').innerHTML = isConnected 
          ? '<span style="color:#10b981;">✅ Connected</span>' 
          : '<span style="color:#ef4444;">❌ Disconnected</span>';
        document.getElementById('currentSsid').textContent = data.wifi.ssid || 'None';
      } catch (e) {
        console.error('Failed to load status:', e);
      }
    }

    // Load configured networks
    async function loadNetworks() {
      try {
        const res = await fetch('/api/networks/configured');
        if (!res.ok) throw new Error('Networks request failed');
        const data = await res.json();
        networks = data.networks || [];
        renderNetworks();
      } catch (e) {
        console.error('Failed to load networks:', e);
        document.getElementById('networkList').innerHTML = 
          '<div class="empty-state"><div class="empty-state-icon">❌</div><div>Failed to load networks</div></div>';
      }
    }

    // Render network list
    function renderNetworks() {
      const container = document.getElementById('networkList');
      
      if (networks.length === 0) {
        container.innerHTML = 
          '<div class="empty-state"><div class="empty-state-icon">📡</div><div>No networks configured yet<br><small>Add one below to get started!</small></div></div>';
        return;
      }

      container.innerHTML = networks.map(net => {
        const ssidDisplay = net.ssid || '<em>(empty)</em>';
        const ssidSafe = (net.ssid || '').replace(/'/g, "\\'");
        const isDisabled = !net.isActive;
        
        return `
        <div class="network ${net.isCurrent ? 'current' : ''}">
          <div class="network-header">
            <div class="network-title">
              <div class="network-ssid">${ssidDisplay} ${net.hasPassword ? '🔒' : '🔓'}</div>
              <div class="network-badges">
                ${net.isCurrent ? '<span class="badge badge-success">Current</span>' : ''}
                ${net.isActive ? '<span class="badge badge-info">Active</span>' : '<span class="badge badge-warning">Inactive</span>'}
              </div>
            </div>
            <div class="network-id">${net.id}</div>
          </div>
          <div class="network-actions">
            <button onclick="connectToNetwork('${net.id}')" ${isDisabled ? 'disabled' : ''}>
              ${net.isCurrent ? '✓ Connected' : '📡 Connect'}
            </button>
            ${net.id !== 'iwcWifi0' ? 
              `<button class="danger" onclick="deleteNetwork('${net.id}', '${ssidSafe}')">🗑️ Delete</button>` 
              : ''}
          </div>
        </div>`;
      }).join('');
    }

    // Add new network (Phase 3 API - auto-slot)
    async function addNetwork(event) {
      event.preventDefault();
      
      const ssid = document.getElementById('addSsid').value.trim();
      const password = document.getElementById('addPassword').value;
      
      if (!ssid) {
        showMessage('addMessage', '❌ SSID is required', 'error');
        return;
      }
      
      showMessage('addMessage', '⏳ Adding network...', 'info');
      
      try {
        const res = await fetch('/api/networks', {
          method: 'POST',
          headers: { 'Content-Type': 'application/json' },
          body: JSON.stringify({ ssid, password })
        });
        
        const data = await res.json();
        
        if (!res.ok) {
          showMessage('addMessage', `❌ Error: ${data.error || 'Failed to add network'}`, 'error');
          return;
        }
        
        showMessage('addMessage', 
          `✅ Network added to slot ${data.slot || 'unknown'}! Status: ${data.status}`, 
          'success'
        );
        
        // Clear form
        document.getElementById('addForm').reset();
        
        // Reload network list after a brief delay
        setTimeout(() => {
          loadStatus();
          loadNetworks();
        }, 2000);
        
      } catch (e) {
        console.error('Add network error:', e);
        showMessage('addMessage', `❌ Error: ${e.message}`, 'error');
      }
    }

    // Delete network (Phase 3 API)
    async function deleteNetwork(id, ssid) {
      if (!confirm(`Delete network "${ssid}"?\n\nThis will remove the credentials from slot ${id}.`)) {
        return;
      }
      
      showMessage('networkMessage', `⏳ Deleting ${id}...`, 'info');
      
      try {
        const res = await fetch(`/api/networks/${id}`, {
          method: 'DELETE'
        });
        
        const data = await res.json();
        
        if (!res.ok) {
          showMessage('networkMessage', `❌ Error: ${data.error || 'Failed to delete'}`, 'error');
          return;
        }
        
        showMessage('networkMessage', `✅ Network "${ssid}" deleted successfully!`, 'success');
        
        // Reload network list
        setTimeout(() => {
          loadNetworks();
        }, 1000);
        
      } catch (e) {
        console.error('Delete network error:', e);
        showMessage('networkMessage', `❌ Error: ${e.message}`, 'error');
      }
    }

    // Connect to a network
    async function connectToNetwork(id) {
      showMessage('networkMessage', '⏳ Connecting to network...', 'info');
      
      try {
        const res = await fetch(`/api/networks/${id}/connect`, { 
          method: 'POST' 
        });
        
        const data = await res.json();
        
        if (res.ok) {
          showMessage('networkMessage', 
            `✅ Connecting to "${data.ssid}"... Please wait.`, 
            'success'
          );
          
          // Poll status after connection attempt
          setTimeout(() => {
            loadStatus();
            loadNetworks();
          }, 4000);
        } else {
          showMessage('networkMessage', `❌ Error: ${data.error}`, 'error');
        }
      } catch (e) {
        console.error('Connect error:', e);
        showMessage('networkMessage', '❌ Connection request failed', 'error');
      }
    }

    // Reconnect WiFi
    async function reconnect() {
      showMessage('networkMessage', '⏳ Reconnecting WiFi...', 'info');
      
      try {
        const res = await fetch('/api/reconnect', { method: 'POST' });
        const data = await res.json();
        
        if (res.ok) {
          showMessage('networkMessage', '✅ WiFi reconnecting... Please wait.', 'success');
          
          setTimeout(() => {
            loadStatus();
            loadNetworks();
          }, 4000);
        } else {
          showMessage('networkMessage', `❌ Error: ${data.error || 'Reconnect failed'}`, 'error');
        }
      } catch (e) {
        console.error('Reconnect error:', e);
        showMessage('networkMessage', '❌ Reconnect request failed', 'error');
      }
    }

    // Reboot device
    async function rebootDevice() {
      if (!confirm('Reboot the device?\n\nYou will need to reconnect after reboot.')) {
        return;
      }
      
      try {
        await fetch('/api/reboot', { method: 'POST' });
        alert('Device is rebooting...\n\nPlease wait 30 seconds and reconnect.');
      } catch (e) {
        // Expected to fail as device reboots
      }
    }

    // Show message
    function showMessage(elementId, message, type) {
      const el = document.getElementById(elementId);
      el.textContent = message;
      el.className = `message ${type}`;
      el.classList.remove('hidden');
      
      // Auto-hide after 8 seconds
      setTimeout(() => {
        el.classList.add('hidden');
      }, 8000);
    }

    // Initial load
    loadStatus();
    loadNetworks();
    
    // Auto-refresh every 15 seconds
    refreshInterval = setInterval(() => {
      loadStatus();
      loadNetworks();
    }, 15000);
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
  
  multiWifi.init();
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
  
  // -- Note: Automatic WiFi scanning is enabled by default
  // The API will scan for networks every 15 seconds to keep the cache fresh.
  // To disable automatic scanning: api.setAutoScanEnabled(false);

  Serial.println("Ready!");
  Serial.print("Access the web interface at: http://");
  Serial.println(WiFi.localIP());
  Serial.println();
  Serial.println("Available Phase 3 API endpoints:");
  Serial.println("  GET    /api/status");
  Serial.println("  GET    /api/networks/configured");
  Serial.println("  POST   /api/networks             (auto-assign slot)");
  Serial.println("  DELETE /api/networks/{id}");
  Serial.println("  POST   /api/networks/{id}/connect");
  Serial.println("  POST   /api/reconnect");
  Serial.println("  GET    /api/config");
  Serial.println("  POST   /api/config");
  Serial.println("  GET    /api/networks             (WiFi scan)");
  Serial.println("  POST   /api/scan");
}

void loop() 
{
  // -- IotWebConf main loop
  iotWebConf.doLoop();
  
  // -- API background tasks (processes async WiFi scans and triggers periodic scans)
  api.loop();
}
