/**
 * IotWebConf20AJsonApiOnly.ino -- IotWebConf is an ESP8266/ESP32
 *   non blocking WiFi/AP web configuration library for Arduino.
 *   https://github.com/prampec/IotWebConf
 *
 * Copyright (C) 2020 Balazs Kelemen <prampec+arduino@gmail.com>
 *
 * This software may be modified and distributed under the terms
 * of the MIT license.  See the LICENSE file for details.
 */

/**
 * Example 20A: JSON API - Pure API Mode
 * 
 * This example demonstrates JSON API network management for developers
 * building custom applications. Focus is on API-first usage without
 * mixing in traditional config portal concepts.
 * 
 * For hybrid usage (API + config portal), see Example 20B.
 * 
 * TARGET USERS:
 * - Mobile app developers managing WiFi via REST API
 * - Custom dashboard/UI builders
 * - Automated provisioning systems
 * - Anyone building JSON API-first applications
 * 
 * WHAT THIS SHOWS:
 * ✅ Pure JSON API usage for network management
 * ✅ Dynamic network addition via POST /api/networks (auto-slot)
 * ✅ Network deletion via DELETE /api/networks/{id}
 * ✅ Network switching via POST /api/networks/{id}/connect
 * ✅ Modern interactive web UI
 * ✅ Clear WiFi storage slot configuration
 * 
 * WHAT THIS DOES NOT SHOW:
 * ❌ Traditional IotWebConf config portal integration
 * ❌ Manual WiFi credential entry via /config page
 * ❌ Hybrid API + config portal usage (see Example 20B)
 * 
 * API ENDPOINTS:
 * - GET    /api/status                   - Device status and current connection
 * - GET    /api/networks/configured      - List all configured networks
 * - POST   /api/networks                 - Add network (auto-assigns slot)
 * - DELETE /api/networks/{id}            - Delete network credentials
 * - POST   /api/networks/{id}/connect    - Connect to specific network
 * - POST   /api/reconnect                - Force WiFi reconnection
 * - GET    /api/networks                 - WiFi scan
 * - POST   /api/scan                     - Trigger WiFi scan
 * 
 * QUICK TEST WITH CURL:
 * 
 *   # List configured networks
 *   curl http://192.168.1.100/api/networks/configured
 * 
 *   # Add new network (auto-assigns to next available slot)
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
 * WEB UI:
 *   Open http://192.168.1.100/ for interactive network management
 * 
 * REQUIREMENTS:
 * - Build flag: -D IOTWEBCONF_ENABLE_JSON (automatically set below)
 * - Library: ArduinoJson v6+
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
#include <IotWebConfMultipleWifi.h>

// -- Initial name, password, and configuration version
const char thingName[] = "ApiDevice";
const char wifiInitialApPassword[] = "apimode123";

#define STRING_LEN 128
#define CONFIG_VERSION "api1"

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
// WIFI STORAGE SLOTS FOR JSON API
// ═══════════════════════════════════════════════════════
// 
// When using the JSON API to manage WiFi networks dynamically,
// you need to define STORAGE SLOTS where network credentials
// will be saved.
// 
// WHAT ARE SLOTS?
// - Each slot stores one WiFi network's SSID and password
// - Slot 0 (iwcWifi0) is the PRIMARY network (always exists)
// - Additional slots allow storing multiple networks
// - Users can add/delete networks via API into these slots
// 
// WHY DO I NEED TO DEFINE SLOTS?
// - Memory must be allocated at compile time
// - Each slot reserves ~256 bytes of EEPROM/NVS storage
// - More slots = more networks users can manage via API
// 
// HOW MANY SLOTS DO I NEED?
// - Minimum: 0 additional slots (only primary network)
// - Typical: 3-5 additional slots (home, office, guest networks)
// - Maximum recommended: 10 additional slots
// - This example: 5 additional slots (6 total including primary)
// 
// HOW TO ADJUST:
// - Change NUM_WIFI_SLOTS below to add/remove slots
// - Update the wifiSlots array accordingly
// - Each slot automatically gets an ID: wifi2, wifi3, wifi4, etc.
// 
// EXAMPLE USER WORKFLOW:
// 1. User calls POST /api/networks {"ssid":"Home","password":"..."}
// 2. API finds first empty slot (e.g., wifi2)
// 3. Credentials saved to that slot
// 4. User can later DELETE or CONNECT to that slot by ID
// 
// ═══════════════════════════════════════════════════════

#define NUM_WIFI_SLOTS 5  // Number of additional WiFi slots (beyond primary)

// Define the storage slots
// These will be accessible via API as: wifi2, wifi3, wifi4, wifi5, wifi6
iotwebconf::ChainedWifiParameterGroup wifiSlots[NUM_WIFI_SLOTS] = {
  {"wifi2"},  // Slot 1: accessible as "wifi2" in API
  {"wifi3"},  // Slot 2: accessible as "wifi3" in API
  {"wifi4"},  // Slot 3: accessible as "wifi4" in API
  {"wifi5"},  // Slot 4: accessible as "wifi5" in API
  {"wifi6"}   // Slot 5: accessible as "wifi6" in API
};

// Register slots with IotWebConf
iotwebconf::MultipleWifiAddition multiWifi(
  &iotWebConf,
  wifiSlots,
  NUM_WIFI_SLOTS
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
  <title>WiFi Network Manager</title>
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
    @keyframes pulse {
      0%, 100% { opacity: 1; }
      50% { opacity: 0.5; }
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
      display: flex;
      align-items: center;
      justify-content: space-between;
    }
    .scan-status {
      font-size: 12px;
      color: #6b7280;
      font-weight: normal;
      animation: pulse 1.5s infinite;
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
    .network-item {
      padding: 14px 16px;
      border: 2px solid #e5e7eb;
      border-radius: 8px;
      margin-bottom: 8px;
      cursor: pointer;
      display: flex;
      justify-content: space-between;
      align-items: center;
      transition: all 0.2s;
      background: white;
    }
    .network-item:hover {
      border-color: #667eea;
      background: #f0f4ff;
      transform: translateX(4px);
    }
    .network-item.selected {
      border-color: #667eea;
      background: #e0e7ff;
      font-weight: 600;
      box-shadow: 0 2px 8px rgba(102,126,234,0.3);
    }
    .network-item-content {
      display: flex;
      align-items: center;
      gap: 12px;
      flex: 1;
    }
    .signal-icon {
      font-size: 20px;
      min-width: 30px;
    }
    .network-name {
      font-size: 16px;
      color: #1a1a1a;
      flex: 1;
    }
    .lock-icon {
      font-size: 16px;
      color: #6b7280;
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
    <h1><span>📶</span> WiFi Network Manager</h1>
    <div class="info">
      <div class="info-line"><strong>Device:</strong> <span id="deviceName">Loading...</span></div>
      <div class="info-line"><strong>IP Address:</strong> <span id="ipAddress">Loading...</span></div>
      <div class="info-line"><strong>Status:</strong> <span id="wifiStatus">Loading...</span></div>
      <div class="info-line"><strong>Current Network:</strong> <span id="currentSsid">Loading...</span></div>
    </div>
  </div>

  <div class="card">
    <h2>
      <span>📡 Available Networks</span>
      <span class="scan-status" id="scanStatus">Scanning...</span>
    </h2>
    <div id="availableNetworks">
      <div class="empty-state">
        <div class="empty-state-icon">🔍</div>
        <div>Scanning for WiFi networks...</div>
      </div>
    </div>
  </div>

  <div class="card">
    <h2>🔐 Connect to Network</h2>
    <form id="addForm" onsubmit="addNetwork(event)">
      <div class="form-group">
        <label for="addSsid">Network Name (SSID) *</label>
        <input type="text" id="addSsid" placeholder="Select a network above or type manually" required>
      </div>
      <div class="form-group" id="passwordGroup">
        <label for="addPassword">Password</label>
        <input type="password" id="addPassword" placeholder="Enter WiFi password">
      </div>
      <button type="submit">🔗 Connect to Network</button>
    </form>
    <div id="addMessage" class="message hidden"></div>
  </div>

  <div class="card">
    <h2>💾 Saved Networks</h2>
    <div id="networkList">
      <div class="empty-state">
        <div class="empty-state-icon">⏳</div>
        <div>Loading saved networks...</div>
      </div>
    </div>
    <div id="networkMessage" class="message hidden"></div>
  </div>

  <div class="card">
    <h2>⚙️ System Actions</h2>
    <div class="system-actions">
      <button class="success" onclick="reconnect()">🔄 Reconnect WiFi</button>
      <button class="warning" onclick="triggerScan()">🔍 Scan Networks</button>
      <button class="danger" onclick="rebootDevice()">🔴 Reboot Device</button>
    </div>
  </div>

  <script>
    let networks = [];
    let networkList = [];  // Persistent list with aging (for available networks)
    let scanInterval = null;
    let refreshInterval = null;
    let selectedNetwork = null;  // Track user-selected SSID

    // Auto-scan on page load
    window.addEventListener('DOMContentLoaded', function() {
      loadStatus();
      loadNetworks();
      scanNetworks();
      
      // Auto-refresh scanned networks every 15 seconds
      scanInterval = setInterval(scanNetworks, 15000);
      
      // Refresh status and saved networks every 10 seconds
      refreshInterval = setInterval(() => {
        loadStatus();
        loadNetworks();
      }, 10000);
    });

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

    // Scan for available networks
    async function scanNetworks() {
      const scanStatus = document.getElementById('scanStatus');
      scanStatus.textContent = 'Scanning...';
      scanStatus.style.display = 'inline';
      scanStatus.style.animation = 'pulse 1.5s infinite';
      
      try {
        // Get scan results (backend returns cached results during scan)
        const res = await fetch('/api/networks');
        if (!res.ok) throw new Error('Scan request failed');
        const data = await res.json();
        
        // Merge new networks into persistent list
        mergeNetworks(data.networks || []);
        
        // Update scanning status
        if (data.scanning) {
          scanStatus.textContent = 'Scanning...';
          scanStatus.style.animation = 'pulse 1.5s infinite';
        } else {
          scanStatus.textContent = 'Last scan: just now';
          scanStatus.style.animation = 'none';
        }
        
        // Re-render with updated list
        renderAvailableNetworks();
      } catch (e) {
        console.error('Failed to scan networks:', e);
        // Keep showing existing networks on error
        scanStatus.textContent = 'Scan failed';
        scanStatus.style.animation = 'none';
      }
    }

    // Merge new networks into persistent list (smart aging)
    function mergeNetworks(newNetworks) {
      const now = Date.now();
      
      // Add/update networks from scan
      newNetworks.forEach(newNet => {
        const existing = networkList.find(n => n.ssid === newNet.ssid);
        
        if (existing) {
          // Update existing network
          existing.rssi = newNet.rssi;
          existing.encryption = newNet.encryption;
          existing.lastSeen = now;
        } else {
          // Add new network
          networkList.push({
            ssid: newNet.ssid,
            rssi: newNet.rssi,
            encryption: newNet.encryption,
            lastSeen: now
          });
        }
      });
      
      // Remove stale networks (not seen for 45+ seconds)
      // BUT: Don't remove if user has selected it
      networkList = networkList.filter(net => {
        const age = now - net.lastSeen;
        const isStale = age > 45000; // 45 seconds = 3 scans
        const isSelected = net.ssid === selectedNetwork;
        
        // Keep if: not stale OR user selected it
        return !isStale || isSelected;
      });
      
      // Sort by signal strength (strongest first)
      networkList.sort((a, b) => b.rssi - a.rssi);
    }

    // Trigger manual scan
    async function triggerScan() {
      showMessage('networkMessage', '🔍 Scanning for networks...', 'info');
      
      try {
        // Trigger backend scan
        await fetch('/api/scan', { method: 'POST' });
        
        // Wait for scan to complete
        await new Promise(resolve => setTimeout(resolve, 3000));
        
        // Get results
        await scanNetworks();
        
        showMessage('networkMessage', '✅ Scan complete!', 'success');
      } catch (e) {
        console.error('Manual scan error:', e);
        showMessage('networkMessage', '❌ Scan failed', 'error');
      }
    }

    // Render available networks
    function renderAvailableNetworks() {
      const container = document.getElementById('availableNetworks');
      
      if (networkList.length === 0) {
        container.innerHTML = 
          '<div class="empty-state"><div class="empty-state-icon">📡</div><div>No networks found<br><small>Scanning...</small></div></div>';
        return;
      }

      container.innerHTML = networkList.map(net => {
        const ssid = net.ssid || '(Hidden Network)';
        const rssi = net.rssi || -100;
        const isSecured = net.encryption !== 0;
        const signalIcon = getSignalIcon(rssi);
        const lockIcon = isSecured ? '🔒' : '🔓';
        const isSelected = ssid === selectedNetwork;
        
        return `
        <div class="network-item ${isSelected ? 'selected' : ''}" 
             data-ssid="${escapeHtml(ssid)}"
             onclick="selectNetwork('${escapeHtml(ssid)}', ${isSecured})">
          <div class="network-item-content">
            <span class="signal-icon">${signalIcon}</span>
            <span class="network-name">${escapeHtml(ssid)}</span>
            <span class="lock-icon">${lockIcon}</span>
          </div>
        </div>`;
      }).join('');
    }

    // Get signal strength icon
    function getSignalIcon(rssi) {
      if (rssi > -50) return '📶'; // Excellent
      if (rssi > -60) return '📶'; // Good
      if (rssi > -70) return '📶'; // Fair
      return '📶'; // Weak
    }

    // Select network from available list
    function selectNetwork(ssid, isSecured) {
      selectedNetwork = ssid;
      
      // Auto-fill SSID
      document.getElementById('addSsid').value = ssid;
      
      // Clear password field
      document.getElementById('addPassword').value = '';
      
      // Show password field
      const passwordGroup = document.getElementById('passwordGroup');
      passwordGroup.style.display = 'block';
      
      // Auto-focus password field
      setTimeout(() => document.getElementById('addPassword').focus(), 100);
      
      // Re-render to show selection highlight
      renderAvailableNetworks();
      
      // Scroll to form
      document.getElementById('addForm').scrollIntoView({ behavior: 'smooth', block: 'nearest' });
    }

    // Load configured networks
    async function loadNetworks() {
      try {
        const res = await fetch('/api/networks/configured');
        if (!res.ok) throw new Error('Networks request failed');
        const data = await res.json();
        
        // Filter out empty slots (networks with no SSID)
        networks = (data.networks || []).filter(net => net.ssid && net.ssid.trim() !== '');
        renderNetworks();
      } catch (e) {
        console.error('Failed to load networks:', e);
        document.getElementById('networkList').innerHTML = 
          '<div class="empty-state"><div class="empty-state-icon">❌</div><div>Failed to load saved networks</div></div>';
      }
    }

    // Render saved network list
    function renderNetworks() {
      const container = document.getElementById('networkList');
      
      if (networks.length === 0) {
        container.innerHTML = 
          '<div class="empty-state"><div class="empty-state-icon">📂</div><div>No saved networks<br><small>Connect to a network above to save it</small></div></div>';
        return;
      }

      container.innerHTML = networks.map(net => {
        const ssidDisplay = net.ssid;
        const ssidSafe = (net.ssid || '').replace(/'/g, "\\'");
        const isDisabled = !net.isActive;
        
        return `
        <div class="network ${net.isCurrent ? 'current' : ''}">
          <div class="network-header">
            <div class="network-title">
              <div class="network-ssid">${escapeHtml(ssidDisplay)} ${net.hasPassword ? '🔒' : '🔓'}</div>
              <div class="network-badges">
                ${net.isCurrent ? '<span class="badge badge-success">Connected</span>' : ''}
                <span class="badge badge-info">${net.id}</span>
              </div>
            </div>
          </div>
          <div class="network-actions">
            <button onclick="connectToNetwork('${net.id}')" ${isDisabled || net.isCurrent ? 'disabled' : ''}>
              ${net.isCurrent ? '✓ Connected' : '📡 Connect'}
            </button>
            ${net.id !== 'iwcWifi0' ? 
              `<button class="danger" onclick="deleteNetwork('${net.id}', '${ssidSafe}')">🗑️ Delete</button>` 
              : ''}
          </div>
        </div>`;
      }).join('');
    }

    // Escape HTML to prevent XSS
    function escapeHtml(text) {
      const div = document.createElement('div');
      div.textContent = text;
      return div.innerHTML;
    }

    // Add new network (auto-slot assignment)
    async function addNetwork(event) {
      event.preventDefault();
      
      const ssid = document.getElementById('addSsid').value.trim();
      const password = document.getElementById('addPassword').value;
      
      if (!ssid) {
        showMessage('addMessage', '❌ SSID is required', 'error');
        return;
      }
      
      showMessage('addMessage', '⏳ Connecting to network...', 'info');
      
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
          `✅ Network saved! Connecting to "${ssid}"... Please wait.`, 
          'success'
        );
        
        // Clear form and selection (allow aging out)
        document.getElementById('addForm').reset();
        selectedNetwork = null;
        
        // Re-render to clear selection highlight
        renderAvailableNetworks();
        
        // Reload after connection attempt
        setTimeout(() => {
          loadStatus();
          loadNetworks();
        }, 5000);
        
      } catch (e) {
        console.error('Add network error:', e);
        showMessage('addMessage', `❌ Error: ${e.message}`, 'error');
      }
    }

    // Delete network
    async function deleteNetwork(id, ssid) {
      if (!confirm(`Delete network "${ssid}"?\n\nThis will remove the saved credentials.`)) {
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
          }, 5000);
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
          }, 5000);
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
  Serial.println("Starting Example 20A - JSON API Pure Mode...");

  // -- Set up IotWebConf
  iotWebConf.setStatusPin(STATUS_PIN);
  iotWebConf.setConfigPin(CONFIG_PIN);

  // -- Register WiFi slots with IotWebConf
  multiWifi.init();
  
  // -- Connect API to multiple WiFi support
  api.setMultipleWifiAddition(&multiWifi);

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
  Serial.println("═══════════════════════════════════════════════");
  Serial.println("JSON API ENDPOINTS (Pure API Mode)");
  Serial.println("═══════════════════════════════════════════════");
  Serial.println("GET    /api/status                  - Device status");
  Serial.println("GET    /api/networks/configured     - List networks");
  Serial.println("POST   /api/networks                - Add network (auto-slot)");
  Serial.println("DELETE /api/networks/{id}           - Delete network");
  Serial.println("POST   /api/networks/{id}/connect   - Connect to network");
  Serial.println("POST   /api/reconnect               - Force reconnect");
  Serial.println("GET    /api/networks                - WiFi scan");
  Serial.println("POST   /api/scan                    - Trigger scan");
  Serial.println("═══════════════════════════════════════════════");
}

void loop() 
{
  // -- IotWebConf main loop
  iotWebConf.doLoop();
  
  // -- API background tasks (processes async WiFi scans and triggers periodic scans)
  api.loop();
}
