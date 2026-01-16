/**
 * IotWebConfApi.cpp -- JSON API for IotWebConf
 */

#include "IotWebConfApi.h"

#ifdef IOTWEBCONF_ENABLE_JSON

namespace iotwebconf
{

IotWebConfApi::IotWebConfApi(IotWebConf* iotWebConf, WebServer* server)
{
  _iotWebConf = iotWebConf;
  _server = server;
}

void IotWebConfApi::setConfigSavedCallback(std::function<void()> func)
{
  _configSavedCallback = func;
}

void IotWebConfApi::registerRoutes()
{
  // Register handlers
  _server->on("/api/status", HTTP_GET, [this]() { this->handleApiStatus(); });
  _server->on("/api/networks", HTTP_GET, [this]() { this->handleApiNetworks(); });
  _server->on("/api/wifi", HTTP_POST, [this]() { this->handleApiWifiConnect(); });
  _server->on("/api/config", HTTP_GET, [this]() { this->handleApiGetConfig(); });
  _server->on("/api/config", HTTP_POST, [this]() { this->handleApiSetConfig(); });
  _server->on("/api/scan", HTTP_POST, [this]() { this->handleApiScan(); });
}

void IotWebConfApi::loop()
{
  // -- Handle async scan results
  if (_scanRequested)
  {
    int n = WiFi.scanComplete();
    if (n >= 0)
    {
      _scanRequested = false;
      // We don't do anything here, the results are cached by WiFi class until deleted
    }
    else if (n == -2)
    {
      // Scan failed
      _scanRequested = false;
    }
  }
}

void IotWebConfApi::handleApiStatus()
{
  JsonDocument doc;
  
  doc["state"] = _iotWebConf->getState(); // Enum value
  doc["thingName"] = _iotWebConf->getThingName();
  doc["ip"] = WiFi.localIP().toString();
  doc["mac"] = WiFi.macAddress();
  
  // Wifi connection status
  doc["wifi"]["status"] = WiFi.status();
  doc["wifi"]["ssid"] = WiFi.SSID();
  doc["wifi"]["rssi"] = WiFi.RSSI();
  
  // AP Status
  doc["ap"]["active"] = (WiFi.getMode() & WIFI_AP);
  doc["ap"]["clients"] = WiFi.softAPgetStationNum();
  doc["ap"]["ip"] = WiFi.softAPIP().toString();

  sendJson(200, doc);
}

void IotWebConfApi::handleApiNetworks()
{
  int n = WiFi.scanComplete();
  
  if (n == -2) 
  {
    // Scan not triggered
    JsonDocument doc;
    doc["error"] = "Scan not triggered or in progress";
    sendJson(400, doc);
    return;
  }
  else if (n == -1)
  {
    // Scan in progress
    JsonDocument doc;
    doc["status"] = "scanning";
    sendJson(202, doc); // Accepted/Processing
    return;
  }

  // Use dynamic document for lists, size depends on networks found
  // Allocating enough for ~20 networks. 
  JsonDocument doc;
  JsonArray networks = doc["networks"].to<JsonArray>();

  for (int i = 0; i < n; ++i)
  {
    JsonObject net = networks.add<JsonObject>();
    net["ssid"] = WiFi.SSID(i);
    net["rssi"] = WiFi.RSSI(i);
    net["encryption"] = WiFi.encryptionType(i);
    // net["bssid"] = WiFi.BSSIDstr(i); // Optional
    // net["channel"] = WiFi.channel(i); // Optional
  }
  
  sendJson(200, doc);
}

void IotWebConfApi::handleApiScan()
{
  // Trigger async scan
  WiFi.scanNetworks(true); // true = async
  _scanRequested = true;
  _lastScanTime = millis();
  
  JsonDocument doc;
  doc["status"] = "Scan started";
  sendJson(202, doc);
}

void IotWebConfApi::handleApiWifiConnect()
{
  if (!_server->hasArg("plain")) 
  {
    JsonDocument doc;
    doc["error"] = "Body missing";
    sendJson(400, doc);
    return;
  }

  String body = _server->arg("plain");
  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, body);

  if (error) 
  {
    JsonDocument errDoc;
    errDoc["error"] = "Invalid JSON";
    sendJson(400, errDoc);
    return;
  }

  const char* ssid = doc["ssid"];
  const char* password = doc["password"];

  if (!ssid) 
  {
    JsonDocument errDoc;
    errDoc["error"] = "SSID missing";
    sendJson(400, errDoc);
    return;
  }

  // Update IotWebConf parameters
  // Note: This modifies the RAM values. To persist, we might need to save.
  // Accessing WifiParameterGroup to update values
  ParameterGroup* wifiGroup = _iotWebConf->getWifiParameterGroup();
  
  // We need to find the SSID and Password parameters. 
  Parameter* ssidParam = _iotWebConf->getWifiSsidParameter();
  Parameter* pwdParam = _iotWebConf->getWifiPasswordParameter();

  strncpy(ssidParam->valueBuffer, ssid, ssidParam->getLength());
  if (password) {
      strncpy(pwdParam->valueBuffer, password, pwdParam->getLength());
  } else {
      // Open network? Or keep existing? Usually means open or empty.
      pwdParam->valueBuffer[0] = '\0';
  }

  // Save the config to EEPROM
  _iotWebConf->saveConfig();
  
  // Trigger connection
  _iotWebConf->forceWifiConnection();

  JsonDocument resDoc;
  resDoc["status"] = "Connecting";
  sendJson(200, resDoc);
}

void IotWebConfApi::handleApiGetConfig()
{
  // Serialize all parameters to JSON
  // We can use the load/saveToJson methods if IOTWEBCONF_ENABLE_JSON is defined
  
  JsonDocument doc;
  JsonObject root = doc.to<JsonObject>();
  
  ParameterGroup* group = _iotWebConf->getRootParameterGroup();
  group->saveToJson(root);
  
  sendJson(200, doc);
}

void IotWebConfApi::handleApiSetConfig()
{
  if (!_server->hasArg("plain")) 
  {
    JsonDocument doc;
    doc["error"] = "Body missing";
    sendJson(400, doc);
    return;
  }

  String body = _server->arg("plain");
  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, body);

  if (error) 
  {
    JsonDocument errDoc;
    errDoc["error"] = "Invalid JSON";
    sendJson(400, errDoc);
    return;
  }

  JsonObject root = doc.as<JsonObject>();
  _iotWebConf->getRootParameterGroup()->loadFromJson(root);
  
  // Save to EEPROM
  _iotWebConf->saveConfig();
  
  if (_configSavedCallback) {
      _configSavedCallback();
  }

  JsonDocument resDoc;
  resDoc["status"] = "Saved";
  sendJson(200, resDoc);
}


void IotWebConfApi::sendJson(int code, JsonDocument& doc)
{
  String response;
  serializeJson(doc, response);
  _server->sendHeader("Access-Control-Allow-Origin", "*");
  _server->send(code, "application/json", response);
}

} // namespace iotwebconf

#endif // IOTWEBCONF_ENABLE_JSON
