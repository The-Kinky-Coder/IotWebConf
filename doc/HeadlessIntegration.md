# Headless Integration & API Guide

This guide explains how to use `IotWebConf` as a backend API provider while serving your own custom HTML/JS frontend (e.g., from SPIFFS/LittleFS or an SD card).

## Concept
By default, `IotWebConf` acts as a monolithic application: it handles the route, processes the data, and renders the HTML view.

To separate this into a **Frontend** (your static HTML/JS) and **Backend** (IotWebConf), you need to understand two things:
1.  **The Write API:** How to send data to IotWebConf using AJAX/Fetch.
2.  **The Read API:** How to get current settings into your JS (since `handleConfig` usually renders HTML).

---

## 1. The Write API (Saving Config)

`IotWebConf` does not use JSON for saving. It uses standard HTTP POST Form Data (`application/x-www-form-urlencoded`).

### Endpoint Logic
When you call `iotWebConf.handleConfig()`, it checks for a POST parameter named `iotSave`.
*   **Method:** `POST`
*   **Content-Type:** `application/x-www-form-urlencoded`
*   **Authentication:** Basic Auth (`admin`: `your-ap-password`)

### Required POST Parameters
| Parameter | Value | Description |
|-----------|-------|-------------|
| `iotSave` | `true` | **Required.** Tells IotWebConf to parse and save the data. |
| `[paramId]` | `[value]` | The ID you gave your `Parameter` (e.g., `wifiSsid`). |

### JavaScript Example (Fetch API)

```javascript
const params = new URLSearchParams();
params.append('iotSave', 'true'); // Crucial!
params.append('iwcWifiSsid', 'MyNetwork');
params.append('iwcWifiPassword', 'secret');
params.append('myCustomParam', '12345');

// For checkboxes, IotWebConf expects the value "selected" if checked.
// If unchecked, do not append the parameter at all.
params.append('myCheckbox', 'selected'); 

fetch('/config', {
    method: 'POST',
    headers: {
        'Content-Type': 'application/x-www-form-urlencoded',
        'Authorization': 'Basic ' + btoa('admin:YOUR_AP_PASS') // Only if needed explicitly
    },
    body: params
})
.then(response => {
    if (response.ok) console.log("Saved!");
});
```

### Controlling the Response (The "Refactor" Benefit)
By default, after a successful POST, `IotWebConf` will try to render the standard HTML "Saved" page. Since you are doing AJAX, you probably want a JSON response instead.

You can achieve this by creating a **Headless Renderer**.

```cpp
class JsonApiResponseRenderer : public IotWebConf::StandardHtmlRenderer
{
protected:
    // Override the method that renders the "Configuration saved" page
    void renderConfigSaved(IotWebConf* iotWebConf, WebRequestWrapper* webRequestWrapper) override
    {
        // Return JSON instead of HTML
        webRequestWrapper->sendHeader("Content-Type", "application/json");
        webRequestWrapper->sendHeader("Cache-Control", "no-cache");
        
        // You can check iotWebConf->getState() here to see if reboot is needed
        webRequestWrapper->send(200, "application/json", "{\"status\":\"saved\"}");
    }
};

// In setup():
JsonApiResponseRenderer myApiRenderer;
iotWebConf.setHtmlRenderer(&myApiRenderer);
```

---

## 2. The Read API (Getting Settings)

`IotWebConf` v3.2.0 does not have a built-in "Get Settings" JSON endpoint enabled by default. You have two options:

### Option A: Use ArduinoJson (Recommended)
If you enable `IOTWEBCONF_ENABLE_JSON` in your build flags, `ParameterGroup` objects gain `saveToJson` methods.

1.  Add `-D IOTWEBCONF_ENABLE_JSON` to `platformio.ini`.
2.  Create a dedicated endpoint in your `WebServer` setup.

```cpp
server.on("/api/settings", HTTP_GET, []() {
    // Check authentication
    if (!iotWebConf.handleCaptivePortal()) { 
        // handleCaptivePortal handles auth challenges automatically
        return; 
    }

    DynamicJsonDocument doc(2048);
    // Serialize all parameters into the JSON doc
    iotWebConf.getRootParameterGroup()->saveToJson(doc);

    String jsonString;
    serializeJson(doc, jsonString);
    server.send(200, "application/json", jsonString);
});
```

### Option B: Manual Construction
If you don't want to rely on `ArduinoJson`, you can manually construct a JSON string using the value buffers.

```cpp
server.on("/api/settings", HTTP_GET, []() {
    String s = "{";
    s += "\"ssid\":\"" + String(iotWebConf.getWifiSsidParameter()->valueBuffer) + "\",";
    // ... add your other parameters
    s += "\"custom\":\"" + String(myCustomParamValue) + "\"";
    s += "}";
    server.send(200, "application/json", s);
});
```

---

## 3. Putting it together: File System & API

Here is how you structure your `setup()` to serve static files from Flash (LittleFS) while using `IotWebConf` purely as an API.

```cpp
void setup() {
    LittleFS.begin();
    // ... setup iotWebConf ...

    // 1. Set the renderer to JSON mode (see section 1)
    iotWebConf.setHtmlRenderer(&myApiRenderer);

    // 2. API Route for SAVING
    // We map "/api/config" to the standard IotWebConf handler
    server.on("/api/config", []{ iotWebConf.handleConfig(); });

    // 3. API Route for READING (see section 2)
    server.on("/api/settings", HTTP_GET, handleGetSettings);

    // 4. Serve Static Files (The Frontend)
    // Any request not handled above falls through to this
    server.onNotFound([]() {
        if (iotWebConf.handleCaptivePortal()) return; // Handle Captive Portal redirects first

        String path = server.uri();
        if (path.endsWith("/")) path += "index.html";
        
        if (LittleFS.exists(path)) {
            File file = LittleFS.open(path, "r");
            server.streamFile(file, getContentType(path));
            file.close();
        } else {
            iotWebConf.handleNotFound(); // 404
        }
    });
}
```

## Summary of APIs

| URL | Method | Action | Implementation |
|-----|--------|--------|----------------|
| `/api/settings` | `GET` | Retrieve JSON config | **User Implemented** (using `saveToJson`) |
| `/api/config` | `POST` | Update config | `iotWebConf.handleConfig()` |
| `/` | `GET` | Serve `index.html` | `server.serveStatic` / `LittleFS` |

