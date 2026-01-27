# IotWebConf Examples Guide

This guide helps you choose the right example for your use case.

---

## 📚 Quick Navigation

### Basic Examples
- [Example 01: Minimal](#example-01-minimal) - Getting started
- [Example 02: Status and Reset](#example-02-status-and-reset) - LED indicators
- [Example 03: Custom Parameters](#example-03-custom-parameters) - User configuration

### Network Management (20A vs 20B vs 20)
- [Example 20A: JSON API Only](#example-20a-json-api-only-recommended) - **Pure API mode** ⭐
- [Example 20B: Hybrid Mode](#example-20b-hybrid-mode-recommended) - **API + Config Portal** ⭐
- [Example 20: Network Management](#example-20-network-management-deprecated) - Original (deprecated)

### Advanced Features
- [Example 15: Multiple WiFi](#example-15-multiple-wifi) - Basic multiple network support
- [Example 19: AsyncWebServer](#example-19-asyncwebserver) - Async server integration
- [All Examples Summary](#all-examples-summary)

---

## 🆕 Network Management Examples: Which One?

We split the original Example 20 into **two focused examples** based on user feedback:

### 🎯 Quick Decision Guide

```
┌─────────────────────────────────────────┐
│   Do you need BOTH JSON API AND        │
│   traditional config portal (/config)?  │
└─────────────────┬───────────────────────┘
                  │
        ┌─────────┴─────────┐
        │                   │
       YES                 NO
        │                   │
        ▼                   ▼
   Example 20B         Example 20A
   (Hybrid Mode)       (API Only)
```

---

## Example 20A: JSON API Only ⭐ RECOMMENDED

**Path:** `examples/IotWebConf20AJsonApiOnly/`

### 🎯 Use This If You Want:
- ✅ Pure JSON REST API control
- ✅ Custom mobile app or dashboard
- ✅ Programmatic network management only
- ✅ Minimal setup and configuration
- ✅ Modern web UI for network management

### ❌ Don't Use This If:
- You need the traditional config portal (/config page)
- End users need to manually enter WiFi credentials via web forms
- You want both API and portal available → Use Example 20B instead

### 📦 What's Included:
- Clean, API-focused code structure
- 5 configurable WiFi storage slots (easily adjustable)
- Interactive web UI for network management
- Complete JSON API endpoints:
  - `GET /api/networks/configured` - List networks
  - `POST /api/networks` - Add network (auto-slot)
  - `DELETE /api/networks/{id}` - Remove network
  - `POST /api/networks/{id}/connect` - Switch network
  - `POST /api/reconnect` - Force reconnection

### 💡 Perfect For:
- Mobile app developers
- IoT dashboard builders
- Automated provisioning systems
- Developer tools and utilities
- API-first architectures

### 📖 Documentation:
See `examples/IotWebConf20AJsonApiOnly/README.md` for:
- Complete API reference
- WiFi slot explanation (what, why, how)
- Real-world usage examples
- Security recommendations
- Troubleshooting guide

---

## Example 20B: Hybrid Mode ⭐ RECOMMENDED

**Path:** `examples/IotWebConf20BJsonApiHybrid/`

### 🎯 Use This If You Want:
- ✅ BOTH JSON API and config portal
- ✅ Manual network entry via web forms
- ✅ Programmatic control via API
- ✅ Maximum flexibility for different users
- ✅ Understanding how both methods work together

### ❌ Don't Use This If:
- You only need JSON API → Use Example 20A instead (simpler)
- You don't need programmatic control → Use Example 15 instead

### 📦 What's Included:
- Full JSON API support (same as 20A)
- Traditional config portal integration
- Shared storage demonstration
- Interactive web UI + config portal
- Detailed documentation on when to use each method

### 🔑 Key Concept: Shared Storage

**Both methods use THE SAME storage slots:**

```
┌─────────────────────────────────────────┐
│     WiFi Storage Slots (EEPROM/NVS)    │
│  slot0: iwcWifi0 (primary)             │
│  slot1: wifi2                           │
│  slot2: wifi3                           │
│  slot3: wifi4                           │
└─────────────┬───────────────────────────┘
              │
      ┌───────┴───────┐
      │               │
   Read by         Read by
      │               │
      ▼               ▼
┌──────────┐    ┌──────────┐
│ Config   │    │   JSON   │
│ Portal   │    │   API    │
│ /config  │    │ /api/*   │
└──────────┘    └──────────┘
```

Users can add networks via `/config` page, and your app can see them via API!

### 💡 Perfect For:
- Consumer IoT products
- Smart home devices
- Products with both technical and non-technical users
- Learning how config portal and API integrate
- Multi-location deployments

### 📖 Documentation:
See `examples/IotWebConf20BJsonApiHybrid/README.md` for:
- Shared storage concept explained
- Config portal usage guide
- JSON API reference
- 5 detailed usage scenarios
- When to use portal vs API
- Real-world use cases

---

## Example 20: Network Management (DEPRECATED)

**Path:** `examples/IotWebConf20NetworkManagement/`

### ⚠️ Status: Deprecated

**Why Deprecated:**
The original Example 20 mixed JSON API and config portal concepts in a way that caused confusion:
- ❓ "Why do I need to hardcode wifi2Group if I'm using the API?"
- ❓ "Is the API limited to slots I define?"
- ❓ "When do I use API vs portal?"

**What to Use Instead:**
- For API-only: Use **Example 20A**
- For hybrid: Use **Example 20B**

This example may be removed in future versions.

---

## 🔍 Detailed Comparison: 20A vs 20B vs 20

| Feature | 20A (API Only) | 20B (Hybrid) | 20 (Original) |
|---------|----------------|--------------|---------------|
| **JSON API** | ✅ Yes | ✅ Yes | ✅ Yes |
| **Config Portal** | ❌ No | ✅ Yes | ⚠️ Unclear |
| **Target User** | API developers | Mixed users | Generic |
| **Code Complexity** | Low (focused) | Medium | High (mixed) |
| **Documentation** | API-focused | Shared storage | Generic |
| **WiFi Slot Clarity** | ⭐⭐⭐⭐⭐ | ⭐⭐⭐⭐⭐ | ⭐⭐ |
| **Use Case Examples** | 4 concrete | 4 concrete | General |
| **Web UI** | Modern, API-focused | Hybrid mode notice | Generic |
| **Status** | ✅ Active | ✅ Active | ⚠️ Deprecated |

---

## 📋 All Examples Summary

### Getting Started
- **01 Minimal** - Bare minimum WiFi configuration
- **02 Status and Reset** - LED status indicators
- **03 Custom Parameters** - Add custom config fields
- **03 Typed Parameters** - Type-safe parameters
- **04 Update Server** - OTA firmware updates
- **05 Callbacks** - Event handlers

### Advanced Configuration
- **06 MQTT App** - MQTT integration
- **07 MQTT Relay** - MQTT-controlled relay
- **08 Web Relay** - Web-controlled relay
- **09 Custom Connection** - Custom WiFi logic
- **10 Custom HTML** - Custom config page HTML
- **11 Advanced Runtime** - Runtime configuration
- **12 Custom Parameter Type** - Custom param types
- **13 Optional Group** - Optional param groups
- **14 Group Chain** - Chained param groups

### Multiple Networks
- **15 Multiple WiFi** - Basic multiple network support (no API)
- **20A JSON API Only** ⭐ - Pure JSON API mode
- **20B Hybrid Mode** ⭐ - JSON API + Config Portal
- ~~**20 Network Management**~~ - (Deprecated - use 20A or 20B)

### Modern Features
- **16 Offline Mode** - Run without WiFi
- **17 JSON Config** - JSON configuration support
- **18 JSON API** - Basic JSON API example
- **19 AsyncWebServer** - Async web server support

### Special
- **99 Bootstrap** - Bootstrap CSS example

---

## 🎓 Learning Path

### Path 1: Web Application Developer (JSON API)
1. Start with **Example 01** - Understand basics
2. Try **Example 18** - Basic JSON API
3. Use **Example 20A** ⭐ - Full network management API
4. Optional: **Example 19** - AsyncWebServer for production

### Path 2: Hybrid Application (API + Portal)
1. Start with **Example 01** - Understand basics
2. Try **Example 15** - Basic multiple WiFi
3. Use **Example 20B** ⭐ - Hybrid mode with both methods

### Path 3: Traditional Config Portal Only
1. Start with **Example 01** - Understand basics
2. Try **Example 03** - Custom parameters
3. Use **Example 15** - Multiple WiFi without API

---

## 🤔 Still Not Sure Which Example?

### Answer These Questions:

**1. Do you need programmatic network management (JSON API)?**
- Yes → Continue to Q2
- No → Use **Example 15** (basic multiple WiFi)

**2. Do end users need to manually configure networks via web form?**
- Yes → Use **Example 20B** (hybrid mode)
- No → Use **Example 20A** (API only)

**3. Are you building a mobile app or custom dashboard?**
- Yes → Use **Example 20A** (clean API focus)

**4. Are you building a consumer product with mixed user types?**
- Yes → Use **Example 20B** (maximum flexibility)

---

## 📞 Need Help?

### Common Questions

**Q: Can I use Example 20A and add config portal later?**
A: Yes! Example 20A can be converted to 20B by enabling config portal. See Example 20B for how.

**Q: What's the difference between Example 18 and 20A?**
A: Example 18 shows basic API concepts. Example 20A is production-ready with full network management, web UI, and auto-slot assignment.

**Q: Why were 20A and 20B created?**
A: The original Example 20 mixed concepts that confused users. 20A and 20B provide clear, focused examples for different use cases.

**Q: How many WiFi slots should I configure?**
A: 
- **Mobile apps:** 5-10 slots (users travel, need multiple networks)
- **Fixed location:** 2-3 slots (home + backup)
- **Multi-location:** 10 slots maximum
- See individual example READMEs for memory considerations

**Q: Can I use both 20A and 20B patterns in the same project?**
A: No, pick one approach. Both do the same thing with different focuses. If you need both API and portal, use 20B.

---

## 📄 Documentation Links

- **Main README:** `README.md`
- **API Documentation:** `doc/JSON-API.md`
- **Example 20A README:** `examples/IotWebConf20AJsonApiOnly/README.md`
- **Example 20B README:** `examples/IotWebConf20BJsonApiHybrid/README.md`
- **Changelog:** `CHANGELOG.md`

---

**Last Updated:** January 27, 2026  
**Library Version:** Phase 3 (Network Management API)
