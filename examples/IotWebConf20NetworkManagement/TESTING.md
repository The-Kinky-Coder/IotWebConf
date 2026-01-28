# IotWebConf20NetworkManagement - Testing Checklist

## Pre-Testing Setup

- [ ] Flash the example to ESP32/ESP8266
- [ ] Connect to "NetworkManager" AP (password: netmgr123)
- [ ] Configure primary WiFi network via captive portal
- [ ] Device connects to WiFi and displays IP in Serial Monitor
- [ ] Open browser to device IP address

---

## Web UI Functionality Tests

### Page Load
- [ ] Page loads without JavaScript errors (check browser console)
- [ ] All sections visible: Status, Networks, Add Network, System Actions
- [ ] Device info displayed correctly in status section
- [ ] Network list loads and displays configured networks

### Status Display
- [ ] Device name shows correctly
- [ ] IP address shows correctly
- [ ] WiFi status shows ✅ Connected (green)
- [ ] Current SSID shows the connected network name

### Network List Display
- [ ] Each network shows SSID (or "(empty)" if blank)
- [ ] Lock icon (🔒/🔓) shows password status correctly
- [ ] Current network has green background and "CURRENT" badge
- [ ] Active networks show "ACTIVE" badge (blue)
- [ ] Inactive networks show "INACTIVE" badge (yellow)
- [ ] Network ID displayed in gray text
- [ ] Primary network (iwcWifi0) has Connect button only
- [ ] Secondary networks have Connect + Delete buttons

### Add Network Form
- [ ] SSID input field accepts text
- [ ] Password input field accepts text and masks input
- [ ] Form validation: shows error if SSID is empty
- [ ] Submit button works when clicked
- [ ] Form submits when Enter is pressed in input field

### Add Network Functionality
- [ ] Click "Add Network" with valid SSID
- [ ] Blue message appears: "⏳ Adding network..."
- [ ] After ~2s, green message appears: "✅ Network added to slot X!"
- [ ] Form fields clear after successful add
- [ ] Network list refreshes after 2s and shows new network
- [ ] Serial Monitor shows connection attempt

### Delete Network Functionality
- [ ] Click "Delete" button on secondary network (e.g., wifi2)
- [ ] Confirmation dialog appears asking to confirm
- [ ] Click OK in confirmation
- [ ] Blue message appears: "⏳ Deleting wifi2..."
- [ ] After ~1s, green message appears: "✅ Network deleted!"
- [ ] Network list refreshes and removed network is gone or shows as inactive
- [ ] Primary network (iwcWifi0) does NOT have Delete button

### Connect Network Functionality
- [ ] Click "Connect" button on an active network
- [ ] Blue message appears: "⏳ Connecting to network..."
- [ ] After ~2s, green message appears: "✅ Connecting to [SSID]..."
- [ ] Status updates after 4s showing new connection
- [ ] Device may disconnect briefly during switch
- [ ] Connected network gets green background and CURRENT badge

### System Actions
- [ ] Click "Reconnect WiFi" button
- [ ] Blue message appears: "⏳ Reconnecting WiFi..."
- [ ] After ~2s, green message appears: "✅ WiFi reconnecting..."
- [ ] Status refreshes after 4s
- [ ] Click "Configure Device" navigates to /config page
- [ ] Click "Reboot Device" shows confirmation dialog
- [ ] Confirm reboot shows alert about 30s wait time

### Error Handling
- [ ] Try adding network with empty SSID → shows red error message
- [ ] Try connecting to inactive network → button is disabled
- [ ] Try deleting network while disconnected → handles gracefully
- [ ] Network request failure shows red error message

### Auto-Refresh
- [ ] Leave page open for 15+ seconds
- [ ] Status section refreshes automatically
- [ ] Network list refreshes automatically
- [ ] No JavaScript errors in console during refresh

### Message System
- [ ] Success messages are green with ✅
- [ ] Error messages are red with ❌
- [ ] Info messages are blue with ⏳
- [ ] Messages auto-hide after 8 seconds
- [ ] Multiple messages don't overlap (each section has own message area)

### Responsive Design
- [ ] Test on desktop browser (wide screen)
- [ ] Test on mobile browser (narrow screen)
- [ ] All buttons remain usable on mobile
- [ ] Forms are usable on mobile
- [ ] Text is readable on both sizes
- [ ] Network cards stack nicely on mobile

---

## API Tests (via curl or Postman)

Replace `192.168.1.100` with your device's IP address.

### GET /api/networks/configured
```bash
curl http://192.168.1.100/api/networks/configured
```
- [ ] Returns 200 OK
- [ ] JSON has "networks" array
- [ ] Each network has: id, ssid, isActive, hasPassword, isCurrent

### POST /api/networks (Add Network)
```bash
curl -X POST http://192.168.1.100/api/networks \
  -H "Content-Type: application/json" \
  -d '{"ssid":"TestNetwork","password":"testpass123"}'
```
- [ ] Returns 200 OK
- [ ] JSON has: status, id, slot, ssid
- [ ] status is "connecting" or "connected"
- [ ] slot number matches expected next slot
- [ ] Network appears in configured list

### POST /api/networks (Add without password)
```bash
curl -X POST http://192.168.1.100/api/networks \
  -H "Content-Type: application/json" \
  -d '{"ssid":"OpenNetwork"}'
```
- [ ] Returns 200 OK
- [ ] Network added successfully
- [ ] hasPassword is false in configured list

### DELETE /api/networks/{id}
```bash
curl -X DELETE http://192.168.1.100/api/networks/wifi2
```
- [ ] Returns 200 OK
- [ ] JSON has: status="deleted", id
- [ ] Network removed from configured list (or shows as inactive)

### DELETE /api/networks/iwcWifi0 (should fail)
```bash
curl -X DELETE http://192.168.1.100/api/networks/iwcWifi0
```
- [ ] Returns 400 Bad Request
- [ ] JSON has error message about primary network

### POST /api/networks/{id}/connect
```bash
curl -X POST http://192.168.1.100/api/networks/wifi2/connect
```
- [ ] Returns 200 OK
- [ ] JSON has: status="connecting", id, ssid
- [ ] Device attempts connection (check Serial Monitor)

### POST /api/reconnect
```bash
curl -X POST http://192.168.1.100/api/reconnect
```
- [ ] Returns 200 OK
- [ ] JSON has: status="reconnecting"
- [ ] WiFi reconnects (may see brief disconnect)

---

## Edge Cases & Stress Tests

### Add Multiple Networks
- [ ] Add 3+ networks in quick succession
- [ ] All are assigned different slots
- [ ] All appear in network list
- [ ] No duplicate slot assignments

### Delete and Re-add
- [ ] Delete a network (e.g., wifi2)
- [ ] Add a new network
- [ ] New network reuses the freed slot
- [ ] Slot numbering is correct

### Fill All Slots
- [ ] Add networks until all slots are full
- [ ] Try adding one more network
- [ ] Should return error: "No available network slots"

### Network Switching
- [ ] Connect to network A
- [ ] Wait for connection
- [ ] Connect to network B
- [ ] Device switches correctly
- [ ] Status updates to show new network

### Form Validation
- [ ] Try submitting empty SSID → blocked
- [ ] Try SSID with special characters → works
- [ ] Try very long SSID (63 chars) → works
- [ ] Try very long password (63 chars) → works

### Browser Compatibility
- [ ] Test in Chrome/Edge
- [ ] Test in Firefox
- [ ] Test in Safari
- [ ] Test in mobile browsers

### Connection Loss Handling
- [ ] Disconnect device from WiFi
- [ ] Web UI shows disconnected status
- [ ] Reconnect device
- [ ] Web UI updates to connected status
- [ ] No need to reload page

---

## Serial Monitor Checks

During all tests, monitor Serial output for:
- [ ] No error messages or crashes
- [ ] API requests logged (if debug enabled)
- [ ] WiFi connection attempts logged
- [ ] EEPROM write operations logged
- [ ] No memory leaks or stack overflows

---

## Performance Checks

- [ ] Page loads in < 2 seconds
- [ ] API responses return in < 500ms
- [ ] Network list with 3 networks renders instantly
- [ ] Form submission feels responsive
- [ ] No lag when clicking buttons
- [ ] Auto-refresh doesn't cause UI flicker

---

## Final Sign-Off

- [ ] All core functionality works
- [ ] No JavaScript errors in console
- [ ] No crashes or reboots during testing
- [ ] Web UI is usable and intuitive
- [ ] API endpoints work correctly
- [ ] Documentation is accurate
- [ ] Example is ready for users

---

## Testing Environment

**Device:** ______________________  
**Firmware Version:** ______________________  
**Browser:** ______________________  
**Date Tested:** ______________________  
**Tested By:** ______________________  

**Overall Status:** ⬜ PASS / ⬜ FAIL  

**Notes:**
