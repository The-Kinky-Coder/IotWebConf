# WiFi Flicker Fix - Testing Checklist

## Quick Test (2 minutes)

### ✅ Visual Stability Test

1. Open device UI: `http://192.168.x.x/`
2. Watch "Available Networks" section for 60 seconds
3. **PASS**: Networks stay visible, no flickering
4. **FAIL**: Networks disappear/reappear

### ✅ Scan Status Indicator

1. Observe "Scanning..." indicator at top-right
2. Should pulse during active scan (~3 seconds)
3. Should show "Last scan: just now" between scans
4. **PASS**: Indicator updates, networks stay visible
5. **FAIL**: Networks disappear when scanning

---

## Complete Test Suite (10 minutes)

### Test 1: Initial Load
```
□ Page loads → Shows "Scanning..." with empty list
□ After 3s → Networks appear
□ Networks stay visible
```

### Test 2: Continuous Operation
```
□ Wait 15s → Second scan starts
□ Networks stay visible during scan
□ RSSI values update (signal bars may change)
□ No flicker or flash
□ Repeat for 60s total
```

### Test 3: Network Aging (5 min)
```
□ Note a visible network (e.g., "TestAP")
□ Turn off that WiFi router/AP
□ Wait and observe:
  □ 15s: Network still visible ✓
  □ 30s: Network still visible ✓
  □ 45s: Network still visible ✓
  □ 60s: Network removed ✓
```

### Test 4: Selection Protection
```
□ Click on a network
□ SSID field auto-filled
□ Password field focused
□ Network highlighted in list
□ Wait 15s for next scan
□ Network stays highlighted
□ Network NOT removed from list
```

### Test 5: Connection Flow
```
□ Select network → Highlighted
□ Enter password
□ Click "Connect to Network"
□ Highlight clears after connect
□ Form resets
□ Network can age out normally
```

### Test 6: Error Handling
```
□ Disconnect WiFi adapter (simulate scan failure)
□ Page keeps showing last known networks
□ No crash, no blank list
□ Reconnect adapter
□ New scan resumes normally
```

---

## Expected vs Previous Behavior

| Scenario | Previous (Broken) | Current (Fixed) |
|----------|------------------|----------------|
| During scan | List clears → empty | List stays visible |
| Network missing | Disappears instantly | Stays for 45s |
| User selecting | May disappear | Protected until connect |
| Signal change | List flickers | Smooth update |
| Scan failure | Empty list | Shows last results |

---

## Quick Visual Test

Open browser console (F12) and run:

```javascript
// Monitor network list stability
setInterval(() => {
  const count = document.querySelectorAll('.network-item').length;
  console.log('Networks visible:', count);
}, 2000);
```

**Expected**: Count stays constant (e.g., 5 → 5 → 5 → 5)  
**Previous**: Count fluctuates (5 → 0 → 5 → 0)

---

## Automated Test (Optional)

```javascript
// Run in browser console for 60s automated test
let flickerCount = 0;
let lastCount = 0;

const testInterval = setInterval(() => {
  const count = document.querySelectorAll('.network-item').length;
  
  if (count === 0 && lastCount > 0) {
    flickerCount++;
    console.error('❌ FLICKER DETECTED! Count went to zero.');
  }
  
  lastCount = count;
}, 500);

setTimeout(() => {
  clearInterval(testInterval);
  if (flickerCount === 0) {
    console.log('✅ PASS: No flicker detected in 60s');
  } else {
    console.error('❌ FAIL: Detected', flickerCount, 'flickers');
  }
}, 60000);
```

---

## Common Issues

### Issue: Networks still flickering
**Cause**: Browser cache showing old JavaScript  
**Fix**: Hard refresh (Ctrl+Shift+R or Cmd+Shift+R)

### Issue: Networks never appear
**Cause**: WiFi scan not triggered  
**Fix**: Click "Scan Networks" button manually

### Issue: Network stays forever (doesn't age out)
**Cause**: User selected it  
**Fix**: Click "Connect" or select different network

### Issue: Age shown but wrong value
**Cause**: `lastSeen` field from backend  
**Fix**: Normal behavior, shows seconds since last seen

---

## Performance Check

### Memory Usage
```cpp
// In Arduino Serial Monitor:
// Should see ~1-2 KB more usage for cache

Free heap before: XXXXX bytes
Free heap after:  XXXXX bytes (slightly less)
```

**Acceptable**: <5 KB overhead  
**Concerning**: >10 KB overhead

### CPU Usage
```cpp
// Scan should not block loop()
// Max scan time: ~3 seconds
// Loop delay: <100ms
```

---

## Sign-Off Checklist

Before marking fix complete:

- [ ] Visual stability test passed (60s, no flicker)
- [ ] Network aging works (45s timeout)
- [ ] Selection protection works
- [ ] Connection clears selection
- [ ] Error handling graceful
- [ ] No console errors
- [ ] Memory usage acceptable
- [ ] Works on ESP32
- [ ] Works on ESP8266
- [ ] Documentation updated

---

## Reporting Issues

If tests fail, include:

1. **Device**: ESP32/ESP8266
2. **Test failed**: Which checklist item
3. **Browser**: Chrome/Firefox/Safari
4. **Console errors**: Copy from F12 developer tools
5. **Expected**: What should happen
6. **Actual**: What happened

Example:
```
Device: ESP32
Test: Visual Stability Test
Browser: Chrome 120
Error: Networks still disappear every 15s
Expected: Networks stay visible
Actual: List clears during scan
Console: No errors
```

---

**Testing guide version**: 1.0  
**Last updated**: January 2026  
**Fix version**: Example 20A WiFi Flicker Fix
