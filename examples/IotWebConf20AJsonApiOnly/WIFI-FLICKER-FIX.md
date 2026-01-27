# WiFi Network List Flicker Fix - Example 20A

## Problem Solved

**Issue**: WiFi network list was clearing/flickering during rescans, creating an unusable experience.

**Root Causes**:
1. **Backend**: WiFi.scanNetworks() returned empty (-1) during active scans
2. **Frontend**: JavaScript replaced entire network list on each update

**Impact**: Users saw networks appear → disappear → reappear in a flickering pattern every 15 seconds.

---

## Solution Implemented

### Backend Changes (IotWebConfApi)

#### 1. Network Cache Structure

Added persistent cache with aging mechanism:

```cpp
struct CachedNetwork {
  String ssid;
  int rssi;
  int encryption;
  unsigned long lastSeen;  // millis() timestamp
};

std::vector<CachedNetwork> _networkCache;
```

#### 2. Cache Update Logic

**In `loop()`**: Automatically updates cache when scan completes:

```cpp
void IotWebConfApi::loop() {
  if (_scanRequested && WiFi.scanComplete() >= 0) {
    _scanRequested = false;
    updateNetworkCache();
  }
}
```

**Cache Management** (`updateNetworkCache()`):
- ✅ **Add new networks** with current timestamp
- ✅ **Update existing networks** with fresh RSSI and timestamp
- ✅ **Remove stale networks** not seen for 45+ seconds (3 scan cycles)
- ✅ **Free memory** by deleting ESP scan results after caching

#### 3. API Response Changes

**GET /api/networks** now returns:

```json
{
  "networks": [
    {
      "ssid": "HomeWiFi",
      "rssi": -45,
      "encryption": 2,
      "lastSeen": 0
    }
  ],
  "scanning": false
}
```

**Key improvements**:
- ✅ Always returns cached list (never empty)
- ✅ Includes `scanning` status flag
- ✅ Provides `lastSeen` age in seconds

---

### Frontend Changes (JavaScript)

#### 1. Persistent Network List

**Before** (flickering):
```javascript
let availableNetworks = [];

async function scanNetworks() {
  const data = await fetch('/api/networks');
  availableNetworks = data.networks || [];  // ❌ Replaces entire list
  renderAvailableNetworks();
}
```

**After** (stable):
```javascript
let networkList = [];  // Persistent list
let selectedNetwork = null;  // Track user selection

async function scanNetworks() {
  const data = await fetch('/api/networks');
  mergeNetworks(data.networks || []);  // ✅ Merge, don't replace
  renderAvailableNetworks();
}
```

#### 2. Smart Network Merging

```javascript
function mergeNetworks(newNetworks) {
  const now = Date.now();
  
  // Add/update networks from scan
  newNetworks.forEach(newNet => {
    const existing = networkList.find(n => n.ssid === newNet.ssid);
    
    if (existing) {
      // Update existing: RSSI, encryption, timestamp
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
  
  // Remove stale networks (45+ seconds old)
  // BUT: Protect user-selected networks
  networkList = networkList.filter(net => {
    const age = now - net.lastSeen;
    const isStale = age > 45000;
    const isSelected = net.ssid === selectedNetwork;
    
    return !isStale || isSelected;  // Keep if fresh OR selected
  });
  
  // Sort by signal strength
  networkList.sort((a, b) => b.rssi - a.rssi);
}
```

#### 3. User Selection Protection

**Problem**: Network disappears while user is entering password

**Solution**: Track selected SSID and protect it from removal:

```javascript
function selectNetwork(ssid, isSecured) {
  selectedNetwork = ssid;  // Mark as protected
  document.getElementById('addSsid').value = ssid;
  document.getElementById('addPassword').focus();
  renderAvailableNetworks();  // Show selection highlight
}

async function addNetwork(event) {
  // ... connect to network ...
  
  // On success: clear selection (allow aging out)
  selectedNetwork = null;
  document.getElementById('addForm').reset();
  renderAvailableNetworks();  // Clear highlight
}
```

---

## Behavior After Fix

### Scenario 1: Initial Page Load
```
0s:  Page loads → Shows "Scanning..." (empty list OK)
2s:  First scan completes → Networks appear
17s: Second scan → Networks STAY VISIBLE, RSSI updated
32s: Third scan → Networks STAY VISIBLE, RSSI updated
```

### Scenario 2: Network Temporarily Missing
```
0s:  Scan: [HomeWiFi, OfficeNet, GuestWiFi]
15s: Scan: [HomeWiFi, GuestWiFi]  (OfficeNet missing)
     → OfficeNet STILL SHOWN (lastSeen = 0s, age = 15s)
30s: Scan: [HomeWiFi, GuestWiFi]
     → OfficeNet STILL SHOWN (lastSeen = 0s, age = 30s)
45s: Scan: [HomeWiFi, GuestWiFi]
     → OfficeNet REMOVED (lastSeen = 0s, age = 45s+)
```

### Scenario 3: User Selecting Network
```
User clicks "HomeWiFi" → selectedNetwork = "HomeWiFi"
  → SSID field filled
  → Password field focused
  
Scan happens, HomeWiFi temporarily not seen
  → HomeWiFi STAYS IN LIST (protected by selection)
  
User types password, clicks Connect
  → Connection starts
  → selectedNetwork = null (protection released)
  → Now HomeWiFi can age out normally if not seen
```

---

## Technical Details

### Timing Configuration

| Parameter | Value | Rationale |
|-----------|-------|-----------|
| Scan interval | 15 seconds | Balance freshness vs performance |
| Stale threshold | 45 seconds | 3 scan cycles missed = truly gone |
| Scan duration | ~3 seconds | ESP WiFi scan time |

### Memory Usage

**Before**:
- ESP WiFi scan: ~200 bytes per network (temporary)
- Frontend: Recreated on each scan

**After**:
- Backend cache: ~80 bytes per network (persistent)
- Frontend cache: ~100 bytes per network (persistent)
- **Total overhead**: ~180 bytes × 10 networks = ~1.8 KB

**Trade-off**: Minimal memory cost for vastly improved UX

### Edge Cases Handled

✅ **Empty initial scan**: Shows "Scanning..." message  
✅ **Scan failure**: Keeps showing last successful results  
✅ **Network goes offline**: Graceful 45s timeout  
✅ **Network returns**: Immediately restored to list  
✅ **User mid-connection**: Protected from removal  
✅ **Signal strength changes**: Updated in real-time  

---

## Testing Instructions

### 1. Visual Test (Flicker Check)

```bash
# Open browser to device
http://192.168.1.100/

# Watch "Available Networks" section
# Expected: Networks stay visible, no flickering
# Previous: Networks disappear every 15s
```

**Pass criteria**: No visual flicker over 60 seconds

### 2. Network Aging Test

```bash
# 1. Note a network in the list (e.g., "OfficeNet")
# 2. Turn off that WiFi router/AP
# 3. Wait and observe

Expected timeline:
- 0s:  Network visible
- 15s: Network still visible (1 scan missed)
- 30s: Network still visible (2 scans missed)
- 45s: Network still visible (3 scans missed)
- 60s: Network removed (45s threshold reached)
```

### 3. Selection Protection Test

```bash
# 1. Click on a network → SSID auto-filled
# 2. Start typing password
# 3. Wait for next scan (15s)

Expected: Network stays highlighted and in list
Previous: Network might disappear mid-entry
```

### 4. Connection Success Test

```bash
# 1. Select network
# 2. Enter password
# 3. Click "Connect to Network"
# 4. Observe selection

Expected: 
- Selection highlight clears after connect
- Network can now age out normally
```

---

## Files Modified

### Library Files
- `src/IotWebConfApi.h` - Added cache structure and updateNetworkCache()
- `src/IotWebConfApi.cpp` - Implemented caching logic

### Example File
- `examples/IotWebConf20AJsonApiOnly/IotWebConf20AJsonApiOnly.ino`
  - Updated JavaScript network management
  - Added mergeNetworks() function
  - Added selection protection

---

## Backwards Compatibility

✅ **API unchanged**: GET /api/networks still returns same structure  
✅ **Added field**: `scanning` boolean (safe to ignore)  
✅ **Added field**: `lastSeen` integer (safe to ignore)  
✅ **Old clients**: Will work but may still flicker (frontend issue)  

---

## Performance Impact

### Backend
- **CPU**: Negligible (simple vector operations)
- **Memory**: +1.8 KB for 10 cached networks
- **Network**: No change (same scan frequency)

### Frontend
- **CPU**: Reduced (merge vs recreate DOM)
- **Memory**: +1 KB for persistent list
- **UX**: Dramatically improved (no flicker)

---

## Future Improvements

### Potential Enhancements
1. **Configurable stale timeout** (currently hardcoded 45s)
2. **Signal strength animation** (smooth transitions)
3. **Network history** (show "last seen: 2m ago")
4. **Favorite networks** (always show, never age out)

### Not Implemented (By Design)
- ❌ Manual cache clear (use device reboot)
- ❌ Per-network age out times (complexity)
- ❌ Database persistence (overkill for cache)

---

## Conclusion

**Problem**: Flickering network list every 15 seconds  
**Solution**: Backend caching + frontend merging + selection protection  
**Result**: Smooth, stable network list with smart aging  

**User impact**: Professional, polished WiFi management experience

**Developer impact**: Minimal code change, no breaking changes

---

## Support

For issues or questions:
- GitHub: https://github.com/prampec/IotWebConf
- Example: `examples/IotWebConf20AJsonApiOnly/`
- Documentation: See main README.md

---

**Fix completed**: January 2026  
**Tested on**: ESP32, ESP8266  
**Library version**: 3.x (Phase 3)
