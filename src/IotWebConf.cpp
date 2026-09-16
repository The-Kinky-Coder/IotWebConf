/**
 * IotWebConf.cpp -- IotWebConf is an ESP8266/ESP32
 *   non blocking WiFi/AP web configuration library for Arduino.
 *   https://github.com/prampec/IotWebConf
 *
 * Copyright (C) 2020 Balazs Kelemen <prampec+arduino@gmail.com>
 *
 * This software may be modified and distributed under the terms
 * of the MIT license.  See the LICENSE file for details.
 */

#include <EEPROM.h>

#include "IotWebConf.h"

#ifdef ESP32
# include <esp_netif.h>
# include <esp_idf_version.h>
# include <esp_wifi.h>
#endif

#ifdef IOTWEBCONF_CONFIG_USE_MDNS
# ifdef ESP8266
#  include <ESP8266mDNS.h>
# elif defined(ESP32)
#  include <ESPmDNS.h>
# endif
#endif

#define IOTWEBCONF_STATUS_ENABLED ((this->_statusPin >= 0) && this->_blinkEnabled)

////////////////////////////////////////////////////////////////

namespace iotwebconf
{

IotWebConf::IotWebConf(
    const char* defaultThingName, DNSServer* dnsServer, WebServerWrapper* webServerWrapper,
    const char* initialApPassword, const char* configVersion)
{
  this->_thingNameParameter.defaultValue = defaultThingName;
  this->_dnsServer = dnsServer;
  this->_webServerWrapper = webServerWrapper;
  this->_initialApPassword = initialApPassword;
  this->_configVersion = configVersion;
#ifdef IOTWEBCONF_DEBUG_TO_SERIAL
  if ((initialApPassword != nullptr) &&
      (strlen(initialApPassword) > 0) && (strlen(initialApPassword) < 8))
  {
    ESP_LOGW(IOTWEBCONF_LOG_TAG,
             "initialApPassword is shorter than 8 characters -- the AP will refuse to start "
             "with it. Use an empty string for an intentionally open default AP.");
  }
#endif

  this->_apTimeoutParameter.visible = false;
  this->_systemParameters.addItem(&this->_thingNameParameter);
  this->_systemParameters.addItem(&this->_apPasswordParameter);
  this->_systemParameters.addItem(&this->_wifiParameters);
  this->_systemParameters.addItem(&this->_apTimeoutParameter);

  this->_allParameters.addItem(&this->_systemParameters);
  this->_allParameters.addItem(&this->_customParameterGroups);
  this->_allParameters.addItem(&this->_hiddenParameters);

  this->_wifiAuthInfo = {this->_wifiParameters._wifiSsid, this->_wifiParameters._wifiPassword};
}

char* IotWebConf::getThingName()
{
  return this->_thingName;
}

void IotWebConf::setConfigPin(int configPin)
{
  this->_configPin = configPin;
}

void IotWebConf::setStatusPin(int statusPin, int statusOnLevel)
{
  this->_statusPin = statusPin;
  this->_statusOnLevel = statusOnLevel;
}

bool IotWebConf::init()
{
  // -- Setup pins.
  if (this->_configPin >= 0)
  {
    pinMode(this->_configPin, INPUT_PULLUP);
    this->_forceDefaultPassword = (digitalRead(this->_configPin) == LOW);
  }
  if (IOTWEBCONF_STATUS_ENABLED)
  {
    pinMode(this->_statusPin, OUTPUT);
    digitalWrite(this->_statusPin, !this->_statusOnLevel);
  }

  // -- Load configuration from EEPROM.
  bool validConfig = this->loadConfig();
  if (!validConfig)
  {
    // -- No config
    this->_apPassword[0] = '\0';
    this->_wifiParameters._wifiSsid[0] = '\0';
    this->_wifiParameters._wifiPassword[0] = '\0';
  }
  this->_apTimeoutMs = atoi(this->_apTimeoutStr) * 1000;

  return validConfig;
}

void IotWebConf::setupRequestHandlers()
{
#if defined(ESP_ASYNC_WEB_SERVER_H_) || defined(_ESPAsyncWebServer_H_) || defined(IOTWEBCONF_USING_ASYNC)
  // Check if we're using AsyncWebServer
  if (this->_asyncWebServerWrapper._server != nullptr)
  {
    AsyncWebServer* asyncServer = this->_asyncWebServerWrapper._server;
    
    // Root page handler
    asyncServer->on("/", HTTP_GET, [this](AsyncWebServerRequest* request) {
      AsyncWebRequestWrapper wrapper(request);
      
      // Check captive portal first
      if (this->handleCaptivePortal(&wrapper))
      {
        // Request was handled by captive portal
        return;
      }
      
      // In AP or unconfigured mode, redirect to the WiFi settings page.
      // (handleCaptivePortal already redirects non-IP hostnames to the device
      //  IP; this catches requests that arrive at / with the correct IP host.)
      if (this->_state == ApMode || this->_state == NotConfigured)
      {
        request->redirect("/wifisettings.html");
        return;
      }
      
      // Default: send to config page
      this->handleConfig(&wrapper);
    });
    
    // Config page handlers (GET and POST)
    asyncServer->on("/config", HTTP_GET, [this](AsyncWebServerRequest* request) {
      AsyncWebRequestWrapper wrapper(request);
      this->handleConfig(&wrapper);
    });
    
    asyncServer->on("/config", HTTP_POST, [this](AsyncWebServerRequest* request) {
      AsyncWebRequestWrapper wrapper(request);
      this->handleConfig(&wrapper);
    });
    
    // 404 Not Found handler with captive portal support
    asyncServer->onNotFound([this](AsyncWebServerRequest* request) {
      AsyncWebRequestWrapper wrapper(request);
      this->handleNotFound(&wrapper);
    });
    
    IOTWEBCONF_DEBUG_LINE(F("AsyncWebServer request handlers registered"));
    return;
  }
#endif

  // For synchronous WebServer, routes need to be set up manually by the user
  // because WebServer requires on() to be called before begin()
  // Users should call server.on() in their setup() code
  IOTWEBCONF_DEBUG_LINE(F("Synchronous WebServer: register handlers manually before server.begin()"));
}

//////////////////////////////////////////////////////////////////

void IotWebConf::addParameterGroup(ParameterGroup* group)
{
  this->_customParameterGroups.addItem(group);
}

void IotWebConf::addHiddenParameter(ConfigItem* parameter)
{
  this->_hiddenParameters.addItem(parameter);
}

void IotWebConf::addSystemParameter(ConfigItem* parameter)
{
  this->_systemParameters.addItem(parameter);
}

int IotWebConf::initConfig()
{
  int size = this->_allParameters.getStorageSize();
#ifdef IOTWEBCONF_DEBUG_TO_SERIAL
  ESP_LOGI(IOTWEBCONF_LOG_TAG, "Config version: %s, Config size: %d", this->_configVersion, size);
#endif

  return size;
}

/**
 * Load the configuration from the eeprom.
 */
bool IotWebConf::loadConfig()
{
  int size = this->initConfig();
  EEPROM.begin(
    IOTWEBCONF_CONFIG_START + IOTWEBCONF_CONFIG_VERSION_LENGTH + size);

  bool result;
  if (this->testConfigVersion())
  {
    int start = IOTWEBCONF_CONFIG_START + IOTWEBCONF_CONFIG_VERSION_LENGTH;
    IOTWEBCONF_DEBUG_LINE(F("Loading configurations"));
    this->_allParameters.loadValue([&](SerializationData* serializationData)
    {
        this->readEepromValue(start, serializationData->data, serializationData->length);
        start += serializationData->length;
    });
#ifdef IOTWEBCONF_DEBUG_TO_SERIAL
    // debugTo(Serial) replaced by ESP_LOG — see IotWebConfSettings.h
#endif
    result = true;
  }
  else
  {
    IOTWEBCONF_DEBUG_LINE(F("Wrong config version. Applying defaults."));
    this->_allParameters.applyDefaultValue();
#ifdef IOTWEBCONF_DEBUG_TO_SERIAL
    // debugTo(Serial) replaced by ESP_LOG — see IotWebConfSettings.h
#endif

    result = false;
  }

  EEPROM.end();
  return result;
}

void IotWebConf::saveConfig()
{
  int size = this->initConfig();
  if (this->_configSavingCallback != nullptr)
  {
    this->_configSavingCallback(size);
  }
  EEPROM.begin(
    IOTWEBCONF_CONFIG_START + IOTWEBCONF_CONFIG_VERSION_LENGTH + size);

  this->saveConfigVersion();
  int start = IOTWEBCONF_CONFIG_START + IOTWEBCONF_CONFIG_VERSION_LENGTH;
  IOTWEBCONF_DEBUG_LINE(F("Saving configuration"));

  this->_allParameters.storeValue([&](SerializationData* serializationData)
  {
    this->writeEepromValue(start, serializationData->data, serializationData->length);
    start += serializationData->length;
  });

  EEPROM.end();

  this->_apTimeoutMs = atoi(this->_apTimeoutStr) * 1000;

  if (this->_configSavedCallback != nullptr)
  {
    this->_configSavedCallback();
  }
}

void IotWebConf::readEepromValue(int start, byte* valueBuffer, int length)
{
  for (int t = 0; t < length; t++)
  {
    *((char*)valueBuffer + t) = EEPROM.read(start + t);
  }
}
void IotWebConf::writeEepromValue(int start, byte* valueBuffer, int length)
{
  for (int t = 0; t < length; t++)
  {
    EEPROM.write(start + t, *((char*)valueBuffer + t));
  }
}

bool IotWebConf::testConfigVersion()
{
  for (byte t = 0; t < IOTWEBCONF_CONFIG_VERSION_LENGTH; t++)
  {
    if (EEPROM.read(IOTWEBCONF_CONFIG_START + t) != this->_configVersion[t])
    {
      return false;
    }
  }
  return true;
}

void IotWebConf::saveConfigVersion()
{
  for (byte t = 0; t < IOTWEBCONF_CONFIG_VERSION_LENGTH; t++)
  {
    EEPROM.write(IOTWEBCONF_CONFIG_START + t, this->_configVersion[t]);
  }
}

void IotWebConf::setWifiConnectionCallback(std::function<void()> func)
{
  this->_wifiConnectionCallback = func;
}

void IotWebConf::setConfigSavingCallback(std::function<void(int size)> func)
{
  this->_configSavingCallback = func;
}

void IotWebConf::setConfigSavedCallback(std::function<void()> func)
{
  this->_configSavedCallback = func;
}

void IotWebConf::setFormValidator(
  std::function<bool(WebRequestWrapper* webRequestWrapper)> func)
{
  this->_formValidator = func;
}

void IotWebConf::setWifiConnectionTimeoutMs(unsigned long millis)
{
  this->_wifiConnectionTimeoutMs = millis;
}

////////////////////////////////////////////////////////////////////////////////

void IotWebConf::handleConfig(WebRequestWrapper* webRequestWrapper)
{
  if (this->_state == OnLine)
  {
    // -- Authenticate
    if (!webRequestWrapper->authenticate(
            IOTWEBCONF_ADMIN_USER_NAME, this->_apPassword))
    {
      IOTWEBCONF_DEBUG_LINE(F("Requesting authentication."));
      webRequestWrapper->requestAuthentication();
      return;
    }
  }

  bool dataArrived = webRequestWrapper->hasArg("iotSave");
  if (!dataArrived || !this->validateForm(webRequestWrapper))
  {
    this->_htmlRenderer->render(this, webRequestWrapper, dataArrived);
  }
  else
  {
    // -- Save config
    IOTWEBCONF_DEBUG_LINE(F("Updating configuration"));

    this->_systemParameters.update(webRequestWrapper);
    this->_customParameterGroups.update(webRequestWrapper);

    this->saveConfig();
    
    this->_htmlRenderer->renderConfigSaved(this, webRequestWrapper);

/*
    String page = htmlFormatProvider->getHead();
    page.replace("{v}", "Config ESP");
    page += htmlFormatProvider->getScript();
    page += htmlFormatProvider->getStyle();
//    page += _customHeadElement;
    page += htmlFormatProvider->getHeadExtension();
    page += htmlFormatProvider->getHeadEnd();
    page += "Configuration saved. ";
    if (this->_apPassword[0] == '\0')
    {
      page += F("You must change the default AP password to continue. Return "
                "to <a href=''>configuration page</a>.");
    }
    else if (this->_wifiParameters._wifiSsid[0] == '\0')
    {
      page += F("You must provide the local wifi settings to continue. Return "
                "to <a href=''>configuration page</a>.");
    }
    else if (this->_state == NotConfigured)
    {
      page += F("Please disconnect from WiFi AP to continue!");
    }
    else
    {
      page += F("Return to <a href='/'>home page</a>.");
    }
    page += htmlFormatProvider->getEnd();

    webRequestWrapper->sendHeader("Content-Length", String(page.length()));
    webRequestWrapper->send(200, "text/html; charset=UTF-8", page);
*/
  }
}

bool IotWebConf::validateForm(WebRequestWrapper* webRequestWrapper)
{
  // -- Clean previous error messages.
  this->_systemParameters.clearErrorMessage();
  this->_customParameterGroups.clearErrorMessage();

  // -- Call external validator.
  bool valid = true;
  if (this->_formValidator != nullptr)
  {
    valid = this->_formValidator(webRequestWrapper);
  }

  // -- Internal validation.
  int l = webRequestWrapper->arg(this->_thingNameParameter.getId()).length();
  if (3 > l)
  {
    this->_thingNameParameter.errorMessage =
        "Give a name with at least 3 characters.";
    valid = false;
  }
  String apPasswordArg = webRequestWrapper->arg(this->_apPasswordParameter.getId());
  if (!isValidPasswordLength(apPasswordArg.c_str()))
  {
    this->_apPasswordParameter.errorMessage =
        "Password length must be at least 8 characters.";
    valid = false;
  }
  String wifiPasswordArg = webRequestWrapper->arg(this->_wifiParameters.wifiPasswordParameter.getId());
  if (!isValidPasswordLength(wifiPasswordArg.c_str()))
  {
    this->_wifiParameters.wifiPasswordParameter.errorMessage =
        "Password length must be at least 8 characters.";
    valid = false;
  }

#ifdef IOTWEBCONF_DEBUG_TO_SERIAL
  ESP_LOGI(IOTWEBCONF_LOG_TAG, "Form validation result is: %s", valid ? "positive" : "negative");
#endif

  return valid;
}

void IotWebConf::handleNotFound(WebRequestWrapper* webRequestWrapper)
{
  if (this->handleCaptivePortal(webRequestWrapper))
  {
    // If captive portal redirect instead of displaying the error page.
    return;
  }
#ifdef IOTWEBCONF_DEBUG_TO_SERIAL
  ESP_LOGI(IOTWEBCONF_LOG_TAG, "Requested a non-existing page: %s", webRequestWrapper->uri().c_str());
#endif
  String message = "Requested a non-existing page\n\n";
  message += "URI: ";
  message += webRequestWrapper->uri();
  message += "\n";

  webRequestWrapper->sendHeader(
      "Cache-Control", "no-cache, no-store, must-revalidate");
  webRequestWrapper->sendHeader("Pragma", "no-cache");
  webRequestWrapper->sendHeader("Expires", "-1");
  webRequestWrapper->sendHeader("Content-Length", String(message.length()));
  webRequestWrapper->send(404, "text/plain", message);
}

/**
 * Redirect to captive portal if we got a request for another domain.
 * Return true in that case so the page handler do not try to handle the request
 * again. (Code from WifiManager project.)
 */
bool IotWebConf::handleCaptivePortal(WebRequestWrapper* webRequestWrapper)
{
  String host = webRequestWrapper->hostHeader();
  String thingName = String(this->_thingName);
  thingName.toLowerCase();
  if (!isIp(host) && !host.startsWith(thingName))
  {
    IpAddress localIp = webRequestWrapper->localAddress();
    String localIpString = toStringIp(localIp);
    if (localIp.isV6())
    {
      localIpString = String("[") + localIpString + "]";
    }
#ifdef IOTWEBCONF_DEBUG_TO_SERIAL
    ESP_LOGI(IOTWEBCONF_LOG_TAG, "Request for %s redirected to %s:%d",
             host.c_str(), localIpString.c_str(), (int)webRequestWrapper->localPort());
# if defined(ESP32) && defined(CONFIG_LWIP_IPV6)
    if (!localIp.isV6())
    {
      String ipv6Global = WiFi.globalIPv6().toString();
      String ipv6LinkLocal = WiFi.linkLocalIPv6().toString();
      if (ipv6Global.length() > 0)
        ESP_LOGI(IOTWEBCONF_LOG_TAG, "IPv6: %s", ipv6Global.c_str());
      else if (ipv6LinkLocal.length() > 0)
        ESP_LOGI(IOTWEBCONF_LOG_TAG, "IPv6 (LL): %s", ipv6LinkLocal.c_str());
    }
# endif
#endif
    String locationHost = toStringIp(localIp);
    if (localIp.isV6())
    {
      locationHost = String("[") + locationHost + "]";
    }
    webRequestWrapper->sendHeader(
      "Location", String("http://") + locationHost + ":" + webRequestWrapper->localPort() + "/wifisettings.html", true);
    webRequestWrapper->send(302, "text/plain", ""); // Empty content inhibits Content-length header so we have to close the socket ourselves.
    webRequestWrapper->stop(); // Stop is needed because we sent no content length
    return true;
  }
  return false;
}

/** Is this an IP? */
bool IotWebConf::isIp(String str)
{
  String host = str;
  host.trim();
  if (host.startsWith("["))
  {
    int closeBracket = host.indexOf(']');
    if (closeBracket <= 1)
    {
      return false;
    }
    host = host.substring(1, closeBracket);
  }
  else
  {
    int firstColon = host.indexOf(':');
    if (firstColon > 0 && host.indexOf(':', firstColon + 1) < 0)
    {
      host = host.substring(0, firstColon);
    }
  }
  IpAddress parsed = IpAddress::parse(host);
  return parsed.isSet();
}

/** IP to String? */
String IotWebConf::toStringIp(const IpAddress& ip)
{
  return ip.toString(false);
}

/////////////////////////////////////////////////////////////////////////////////

void IotWebConf::delay(unsigned long m)
{
  unsigned long delayStart = millis();
  while (m > millis() - delayStart)
  {
    this->doLoop();
    // -- Note: 1ms might not be enough to perform a full yield. So
    // 'yield' in 'doLoop' is eventually a good idea.
    delayMicroseconds(1000);
  }
}

void IotWebConf::doLoop()
{
  doBlink();
  yield(); // -- Yield should not be necessary, but cannot hurt either.
  if (this->_state == Boot)
  {
    // -- After boot, fall immediately to AP mode.
    NetworkState startupState = ApMode;
    if (this->_startupOffLine)
    {
      startupState = OffLine;
    }
    else if (this->_skipApStartup || this->_autoConnectOnBoot)
    {
      if (mustStayInApMode())
      {
        IOTWEBCONF_DEBUG_LINE(
            F("SkipApStartup is requested, but either no WiFi was set up, or "
              "configButton was pressed."));
      }
      else
      {
        // -- Startup state can be WiFi, if it is requested and also possible.
        if (this->_skipApStartup)
        {
          IOTWEBCONF_DEBUG_LINE(F("SkipApStartup mode was applied"));
        }
        else
        {
          IOTWEBCONF_DEBUG_LINE(F("AutoConnectOnBoot mode was applied"));
        }
        startupState = Connecting;
      }
    }
    this->changeState(startupState);
  }
  else if (
      (this->_state == NotConfigured) ||
      (this->_state == ApMode))
  {
    // -- We must only leave the AP mode, when no slaves are connected.
    // -- Other than that AP mode has a timeout. E.g. after boot, or when retry
    // connecting to WiFi
    checkConnection();
    checkApTimeout();
#ifdef ESP8266
    // DNSServer is async-UDP-based on Arduino-ESP32 3.x; processNextRequest() is a no-op there.
    this->_dnsServer->processNextRequest();
#endif
    this->_webServerWrapper->handleClient();
  }
  else if (this->_state == Connecting)
  {
    if (_apActiveBeforeConnect)
    {
      // AP is still alive in AP_STA mode — keep DNS and web handler ticking
      // so browsers connected via the hotspot can still reach /api/status.
#ifdef ESP8266
      this->_dnsServer->processNextRequest();
#endif
      this->_webServerWrapper->handleClient();
    }
    if (checkWifiConnection())
    {
      this->changeState(OnLine);
      return;
    }
  }
  else if (this->_state == OnLine)
  {
    // Required for mDNS to work on ESP8266
#ifdef IOTWEBCONF_CONFIG_USE_MDNS
# ifdef ESP8266
    MDNS.update();
# endif
#endif
    // -- In server mode we provide web interface. And check whether it is time
    // to run the client.
    this->_webServerWrapper->handleClient();
    // -- Keep DNS alive for AP clients for as long as the AP is up here, independent of the
    //    grace timer -- an AP forced up while already OnLine (forceApMode(true)) has no grace
    //    period but still needs its captive-portal DNS serviced on ESP8266.
    if (_apActiveBeforeConnect)
    {
#ifdef ESP8266
      this->_dnsServer->processNextRequest();
#endif
    }
    // -- AP grace period: shut down AP once the grace period elapses (giving the UI time to show
    //    success), unless the AP is being force-kept-up, in which case the force must be cleared
    //    (via forceApMode(false)/closeAp()) before the AP can go down.
    if ((_apGraceShutdownTime > 0) && !this->_forceApMode)
    {
      if (millis() >= _apGraceShutdownTime)
      {
        _apGraceShutdownTime = 0;
        this->setApKeepAlive(false);
        this->stopApInterface();
        WiFi.mode(WIFI_STA);         // drop to STA-only; STA connection stays up
        ESP_LOGI(IOTWEBCONF_LOG_TAG, "AP grace period elapsed — stopped AP, now STA only.");
      }
    }
    if (WiFi.status() != WL_CONNECTED)
    {
      IOTWEBCONF_DEBUG_LINE(F("Not connected. Try reconnect..."));
      this->changeState(Connecting);
      return;
    }
  }
}

/**
 * What happens, when a state changed...
 */
void IotWebConf::changeState(NetworkState newState)
{
  switch (newState)
  {
    case ApMode:
    {
      // -- In AP mode we must override the default AP password. Otherwise we stay
      // in STATE_NOT_CONFIGURED.
      if (mustUseDefaultPassword())
      {
#ifdef IOTWEBCONF_DEBUG_TO_SERIAL
        if (this->_forceDefaultPassword)
        {
          ESP_LOGI(IOTWEBCONF_LOG_TAG, "AP mode forced by reset pin");
        }
        else
        {
          ESP_LOGI(IOTWEBCONF_LOG_TAG, "AP password was not set in configuration");
        }
#endif
        newState = NotConfigured;
      }
      break;
    }
    default:
      break;
  }
#ifdef IOTWEBCONF_DEBUG_TO_SERIAL
  ESP_LOGI(IOTWEBCONF_LOG_TAG, "State changing from: %d to %d", (int)this->_state, (int)newState);
#endif
  NetworkState oldState = this->_state;
  this->_state = newState;
  this->stateChanged(oldState, newState);
#ifdef IOTWEBCONF_DEBUG_TO_SERIAL
  ESP_LOGI(IOTWEBCONF_LOG_TAG, "State changed from: %d to %d", (int)oldState, (int)newState);
#endif
}

/**
 * Cleanly stopping mDNS after network failure.
 */
void IotWebConf::endMDns(NetworkState oldState)
{
#ifdef IOTWEBCONF_CONFIG_USE_MDNS
  if (oldState == OnLine)
  {
    MDNS.end();
    IOTWEBCONF_DEBUG_LINE(F("Deactivated mDNS until reconnected to WiFi."));
  }
#endif
}

/**
 * What happens, when a state changed...
 */
void IotWebConf::stateChanged(NetworkState oldState, NetworkState newState)
{
//  updateOutput();
  switch (newState)
  {
    case OffLine:
      endMDns(oldState);
      WiFi.disconnect(true);
      WiFi.mode(WIFI_OFF);
      this->blinkInternal(22000, 1320);
      break;
    case ApMode:
    case NotConfigured:
      if (newState == ApMode)
      {
        this->blinkInternal(300, 270);
      }
      else
      {
        this->blinkInternal(300, 150);
      }
      if ((oldState == Connecting) ||
        (oldState == OnLine))
      {
        endMDns(oldState);
        WiFi.disconnect(true);
      }
      // setupAp() no longer sets the WiFi mode itself -- set it here so a keep-alive AP (e.g. a
      // forced AP surviving a failed connection attempt) stays in AP_STA instead of being forced
      // back to pure AP, matching the pattern the Connecting case below already uses.
      WiFi.mode(_apActiveBeforeConnect ? WIFI_AP_STA : WIFI_AP);
      setupAp();
      if (this->_updateServerSetupFunction != nullptr)
      {
        this->_updateServerSetupFunction(this->_updatePath);
      }
      this->_webServerWrapper->begin();
      this->_apConnectionState = NoConnections;
      this->_apStartTimeMs = millis();
#ifdef IOTWEBCONF_DEBUG_TO_SERIAL
      if (mustStayInApMode())
      {
        if (this->_forceDefaultPassword)
          ESP_LOGI(IOTWEBCONF_LOG_TAG, "Default password was forced.");
        if (this->_apPassword[0] == '\0')
          ESP_LOGI(IOTWEBCONF_LOG_TAG, "AP password was not set.");
        if (this->_wifiParameters._wifiSsid[0] == '\0')
          ESP_LOGI(IOTWEBCONF_LOG_TAG, "WiFi SSID was not set.");
        if (this->_forceApMode)
          ESP_LOGI(IOTWEBCONF_LOG_TAG, "AP was forced.");
        ESP_LOGI(IOTWEBCONF_LOG_TAG, "Will stay in AP mode.");
      }
      else
      {
        ESP_LOGI(IOTWEBCONF_LOG_TAG, "AP timeout (ms): %lu", this->_apTimeoutMs);
      }
#endif
      break;
    case Connecting:
    {
      // Was the AP already up before this transition? OR-ing in the *current* value of
      // _apActiveBeforeConnect (not just checking oldState) covers a keep-alive set by any means --
      // including a forced AP started while _state stayed OnLine, and a MultipleWifi slot-to-slot
      // fallback that re-enters Connecting from Connecting -- not just the two states that bring an
      // AP up themselves.
      bool apWasUp = _apActiveBeforeConnect || (oldState == ApMode) || (oldState == NotConfigured);
      // _suppressNextApKeepAlive is a one-shot veto for the one caller (forceApMode(false) leaving
      // ApMode/NotConfigured) that must NOT preserve a keep-alive here even though apWasUp is true.
      bool keepAlive = apWasUp && !_suppressNextApKeepAlive;
      _suppressNextApKeepAlive = false;
      setApKeepAlive(keepAlive);
      _apGraceShutdownTime = 0; // reset any pending grace-period from a prior attempt
      if (keepAlive)
      {
        // Keep the soft-AP alive so the browser can continue polling /api/status while the STA
        // connection is attempted. We set WIFI_AP_STA below instead of tearing the AP down.
        ESP_LOGI(IOTWEBCONF_LOG_TAG, "Keeping AP alive during STA connect (AP_STA mode)");
      }
      else if (apWasUp)
      {
        // AP was up but isn't being carried into this Connecting attempt -- tear down DNS and the
        // soft-AP now, before dropping the radio to STA-only, so DNS doesn't keep listening (#6).
        stopApInterface();
      }
      if ((oldState == Boot) && (this->_updateServerSetupFunction != nullptr))
      {
        // We've skipped AP mode, so update server needs to be set up now.
        this->_updateServerSetupFunction(this->_updatePath);
      }
      endMDns(oldState);
      this->blinkInternal(1000, 500);
#ifdef IOTWEBCONF_DEBUG_TO_SERIAL
# ifdef IOTWEBCONF_DEBUG_PWD_TO_SERIAL
      ESP_LOGI(IOTWEBCONF_LOG_TAG, "Connecting to [%s] with password [%s], timeout %lu ms",
               this->_wifiAuthInfo.ssid, this->_wifiAuthInfo.password, this->_wifiConnectionTimeoutMs);
# else
      ESP_LOGI(IOTWEBCONF_LOG_TAG, "Connecting to [%s] (password hidden), timeout %lu ms",
               this->_wifiAuthInfo.ssid, this->_wifiConnectionTimeoutMs);
# endif
#endif
      this->_wifiConnectionStart = millis();
      this->_wifiRetryCount = 0;
      this->_wifiRetryWaitUntilMs = 0;
      // The order of WiFi.mode and WiFi.setHostname matters based on the platform
#ifdef ESP8266
      WiFi.mode(_apActiveBeforeConnect ? WIFI_AP_STA : WIFI_STA);
      WiFi.hostname(this->_thingName);
#elif defined(ESP32)
      WiFi.setHostname(this->_thingName);
      WiFi.mode(_apActiveBeforeConnect ? WIFI_AP_STA : WIFI_STA);
      // Disable the ESP32 auto-reconnect so it never races IotWebConf's state
      // machine. Without this, the driver starts reconnecting to the last AP
      // the instant WiFi drops — before IotWebConf's doLoop() fires —
      // so our WiFi.begin() arrives while STA is already CONNECTING and returns
      // ESP_ERR_WIFI_STATE, leaving the stack in a dirty state that breaks
      // fallback attempts. IotWebConf manages all reconnection itself.
      WiFi.setAutoReconnect(false);
      // Re-enable IPv6 SLAAC after every WiFi.mode() call. WiFi.mode()
      // re-creates the underlying STA netif, which clears the IPv6 state set
      // by any previous enableIPv6() call (including the one in setup()).
      // Without this, only the very first connection ever gets a global IPv6
      // address; all subsequent reconnects and fallbacks stay IPv4-only.
      WiFi.enableIPv6();
#endif
      this->_wifiConnectionHandler(
          this->_wifiAuthInfo.ssid, this->_wifiAuthInfo.password);
      break;
    }
    case OnLine:
      // -- Initialize mdns after network connection
#ifdef IOTWEBCONF_CONFIG_USE_MDNS
      MDNS.begin(this->_thingName);
      MDNS.addService("http", "tcp", IOTWEBCONF_CONFIG_USE_MDNS);
# ifdef IOTWEBCONF_DEBUG_TO_SERIAL
      ESP_LOGI(IOTWEBCONF_LOG_TAG, "Active mDNS services: %d", MDNS.queryService("http", "tcp"));
# endif
#endif
      this->blinkInternal(8000, 160);
      if (this->_updateServerUpdateCredentialsFunction != nullptr)
      {
        this->_updateServerUpdateCredentialsFunction(
            IOTWEBCONF_ADMIN_USER_NAME, this->_apPassword);
      }
      this->_webServerWrapper->begin();
      IOTWEBCONF_DEBUG_LINE(F("Accepting connection"));
      if (this->_wifiConnectionCallback != nullptr)
      {
        this->_wifiConnectionCallback();
      }
      // If we connected from AP setup mode, keep the hotspot alive for a grace
      // period so the browser can display the success result before the AP drops.
      if (_apActiveBeforeConnect && _apGraceShutdownTime == 0)
      {
        _apGraceShutdownTime = millis() + 90000UL; // 90-second grace period
        ESP_LOGI(IOTWEBCONF_LOG_TAG, "AP grace period started — hotspot stays up for 90 s.");
      }
      break;
    default:
      break;
  }
}

void IotWebConf::checkApTimeout()
{
  if ( !mustStayInApMode() )
  {
    // -- Only move on, when we have a valid WifF and AP configured.
    if ((this->_apConnectionState == Disconnected) ||
        (((millis() - this->_apStartTimeMs) > this->_apTimeoutMs) &&
         (this->_apConnectionState != HasConnection)))
    {
      this->changeState(Connecting);
    }
  }
}

void IotWebConf::goOnLine(bool apMode)
{
  if (this->_state != OffLine)
  {
    IOTWEBCONF_DEBUG_LINE(F("Requested OnLine mode, but was not offline."));
    return;
  }
  if (apMode || mustStayInApMode())
  {
    this->changeState(ApMode);
  }
  else
  {
    this->changeState(Connecting);
  }
}

/**
 * Checks whether we have anyone joined to our AP.
 * If so, we must not change state. But when our guest leaved, we can
 * immediately move on.
 */
void IotWebConf::checkConnection()
{
  if ((this->_apConnectionState == NoConnections) &&
      (WiFi.softAPgetStationNum() > 0))
  {
    this->_apConnectionState = HasConnection;
    IOTWEBCONF_DEBUG_LINE(F("Connection to AP."));
  }
  else if (
      (this->_apConnectionState == HasConnection) &&
      (WiFi.softAPgetStationNum() == 0))
  {
    this->_apConnectionState = Disconnected;
    IOTWEBCONF_DEBUG_LINE(F("Disconnected from AP."));
    if (this->_forceDefaultPassword)
    {
      IOTWEBCONF_DEBUG_LINE(F("Releasing forced AP mode."));
      this->_forceDefaultPassword = false;
    }
  }
}

bool IotWebConf::checkWifiConnection()
{
  wl_status_t currentStatus = WiFi.status();
  
  if (currentStatus != WL_CONNECTED)
  {
    if ((millis() - this->_wifiConnectionStart) > this->_wifiConnectionTimeoutMs)
    {
      // -- Connection timeout reached, check if we should retry
      
      // Classify error as transient or permanent
      bool isTransientError = (currentStatus == WL_IDLE_STATUS || 
                                currentStatus == WL_DISCONNECTED);
      
      bool shouldRetry = isTransientError && 
                         (this->_wifiRetryCount < IOTWEBCONF_WIFI_RETRY_COUNT);
      
      if (shouldRetry)
      {
        if (this->_wifiRetryWaitUntilMs == 0)
        {
          WiFi.disconnect(true);
          this->_wifiRetryWaitUntilMs = millis() + IOTWEBCONF_WIFI_RETRY_DELAY_MS;
        }

        if (millis() < this->_wifiRetryWaitUntilMs)
          return false;

        this->_wifiRetryWaitUntilMs = 0;
        this->_wifiRetryCount++;

#ifdef IOTWEBCONF_DEBUG_TO_SERIAL
        ESP_LOGI(IOTWEBCONF_LOG_TAG, "Transient WiFi error (status=%d) - retrying (%d/%d)",
                 (int)currentStatus, this->_wifiRetryCount, IOTWEBCONF_WIFI_RETRY_COUNT);
#endif

        this->_wifiConnectionStart = millis();
        WiFi.begin(this->_wifiAuthInfo.ssid, this->_wifiAuthInfo.password);
      }
      else
      {
        // -- Permanent error OR retries exhausted, fall back
        if (this->_wifiRetryCount >= IOTWEBCONF_WIFI_RETRY_COUNT)
        {
          IOTWEBCONF_DEBUG_LINE(F("Giving up after retries."));
        }
        else
        {
#ifdef IOTWEBCONF_DEBUG_TO_SERIAL
          ESP_LOGI(IOTWEBCONF_LOG_TAG, "Permanent WiFi error (status=%d) - giving up.", (int)currentStatus);
#endif
        }
        
        WiFi.disconnect(true);
        WifiAuthInfo* newWifiAuthInfo = _wifiConnectionFailureHandler();
        if (newWifiAuthInfo != nullptr)
        {
          // -- Try connecting with another connection info.
          this->_wifiAuthInfo.ssid = newWifiAuthInfo->ssid;
          this->_wifiAuthInfo.password = newWifiAuthInfo->password;
          this->changeState(Connecting);
        }
        else
        {
          this->changeState(ApMode);
        }
      }
    }
    return false;
  }

  // -- Connected
#ifdef IOTWEBCONF_DEBUG_TO_SERIAL
  ESP_LOGI(IOTWEBCONF_LOG_TAG, "WiFi connected");
  ESP_LOGI(IOTWEBCONF_LOG_TAG, "IP address: %s", WiFi.localIP().toString().c_str());
# if defined(ESP32) && defined(CONFIG_LWIP_IPV6)
  {
    String ipv6Global = WiFi.globalIPv6().toString();
    String ipv6LinkLocal = WiFi.linkLocalIPv6().toString();
    if (ipv6Global.length() > 0)
      ESP_LOGI(IOTWEBCONF_LOG_TAG, "IPv6: %s", ipv6Global.c_str());
    else if (ipv6LinkLocal.length() > 0)
      ESP_LOGI(IOTWEBCONF_LOG_TAG, "IPv6 (LL): %s", ipv6LinkLocal.c_str());
  }
# endif
#endif

  return true;
}

void IotWebConf::setupAp()
{
  // Caller sets WiFi.mode() (WIFI_AP or WIFI_AP_STA) before calling this -- setupAp() no longer
  // forces the mode itself, so a keep-alive AP started alongside an active STA connection isn't
  // torn down the moment the AP interface is (re-)configured.

  const char* activePassword = this->activeApPassword();
  size_t activePasswordLen = strlen(activePassword);
  if ((activePasswordLen > 0) && (activePasswordLen < 8))
  {
    ESP_LOGW(IOTWEBCONF_LOG_TAG,
             "Refusing to start AP: active password is %u characters, minimum is 8.",
             (unsigned)activePasswordLen);
    return;
  }

#ifdef IOTWEBCONF_DEBUG_TO_SERIAL
  ESP_LOGI(IOTWEBCONF_LOG_TAG, "Setting up AP: %s", this->_thingName);
# ifdef IOTWEBCONF_DEBUG_PWD_TO_SERIAL
  ESP_LOGI(IOTWEBCONF_LOG_TAG, "Use password: %s", activePassword);
# else
  ESP_LOGI(IOTWEBCONF_LOG_TAG, "Use password: <hidden>");
# endif
#endif

  bool skipConnectionHandler = false;
#ifdef ESP32
  // Skip re-running WiFi.softAP() (via _apConnectionHandler) if the AP is already up with the
  // same SSID/password -- softAP() defaults to channel 1, which would silently reconfigure (and
  // briefly drop clients from) an AP_STA AP that's actually running on the STA's channel. Only
  // the AP still needs the DNS/DHCP-option-114 refresh below, which is cheap and idempotent.
  if ((WiFi.getMode() & WIFI_AP) != 0)
  {
    wifi_config_t conf = {};
    if (esp_wifi_get_config(WIFI_IF_AP, &conf) == ESP_OK)
    {
      bool ssidMatches = strncmp((const char*)conf.ap.ssid, this->_thingName,
                                  sizeof(conf.ap.ssid)) == 0;
      bool passwordMatches = strncmp((const char*)conf.ap.password, activePassword,
                                      sizeof(conf.ap.password)) == 0;
      skipConnectionHandler = ssidMatches && passwordMatches;
    }
  }
#endif
  if (!skipConnectionHandler)
  {
    this->_apConnectionHandler(this->_thingName, activePassword);
  }

#ifdef IOTWEBCONF_DEBUG_TO_SERIAL
  ESP_LOGI(IOTWEBCONF_LOG_TAG, "AP IP address: %s", WiFi.softAPIP().toString().c_str());
# if defined(ESP32) && defined(CONFIG_LWIP_IPV6)
  {
    String ipv6Global = WiFi.globalIPv6().toString();
    String ipv6LinkLocal = WiFi.linkLocalIPv6().toString();
    if (ipv6Global.length() > 0)
      ESP_LOGI(IOTWEBCONF_LOG_TAG, "IPv6: %s", ipv6Global.c_str());
    else if (ipv6LinkLocal.length() > 0)
      ESP_LOGI(IOTWEBCONF_LOG_TAG, "IPv6 (LL): %s", ipv6LinkLocal.c_str());
  }
# endif
#endif
  //  delay(500); // Without delay I've seen the IP address blank
  //  Serial.print(F("AP IP address: "));
  //  Serial.println(WiFi.softAPIP());

  /* Setup the DNS server redirecting all the domains to the apIP.
     Note: DNSServer in use here does not support IPv6 responses, so we
     only configure IPv4 for captive portal DNS. */
  this->_dnsServer->setErrorReplyCode(DNSReplyCode::NoError);
  this->_dnsServer->start(IOTWEBCONF_DNS_PORT, "*", WiFi.softAPIP());

#ifdef ESP32
  // Enable DHCP Option 114 (captive portal URI) — supported from ESP-IDF 5.4.2.
  // Modern OS captive portal detectors (iOS, Android, Windows, macOS) send a
  // DHCP discover and read option 114 to locate the portal without relying on
  // DNS poisoning alone.  We point it directly at wifisettings.html so users
  // land on the friendly WiFi setup page rather than the raw /config form.
# if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 4, 2)
  {
    esp_netif_t* ap_netif = esp_netif_get_handle_from_ifkey("WIFI_AP_DEF");
    if (ap_netif)
    {
      esp_err_t err;
      err = esp_netif_dhcps_stop(ap_netif);
      if (err == ESP_OK || err == ESP_ERR_ESP_NETIF_DHCP_ALREADY_STOPPED)
      {
        int uriLen = snprintf(this->_captivePortalUri, sizeof(this->_captivePortalUri),
                              "http://%s/wifisettings.html",
                              WiFi.softAPIP().toString().c_str());
        if (uriLen < 0 || (size_t)uriLen >= sizeof(this->_captivePortalUri))
        {
          ESP_LOGW(IOTWEBCONF_LOG_TAG, "DHCP captive portal URI too long, option not set");
        }
        else
        {
          err = esp_netif_dhcps_option(ap_netif, ESP_NETIF_OP_SET,
                                       ESP_NETIF_CAPTIVEPORTAL_URI,
                                       (void*)this->_captivePortalUri,
                                       (uint32_t)uriLen);
          if (err != ESP_OK)
          {
            ESP_LOGI(IOTWEBCONF_LOG_TAG, "DHCP captive portal option set failed: 0x%x", err);
          }
          else
          {
            ESP_LOGI(IOTWEBCONF_LOG_TAG, "DHCP captive portal URI: %s", this->_captivePortalUri);
          }
        }
        esp_netif_dhcps_start(ap_netif);
      }
    }
  }
# endif  // ESP_IDF_VERSION
#endif  // ESP32
}

void IotWebConf::stopAp()
{
  WiFi.softAPdisconnect(true);
  WiFi.mode(WIFI_OFF);
}

void IotWebConf::stopApInterface()
{
  this->_dnsServer->stop();
  WiFi.softAPdisconnect(true);
}

void IotWebConf::setApKeepAlive(bool keepAlive)
{
  this->_apActiveBeforeConnect = keepAlive;
}

void IotWebConf::closeAp()
{
  this->stopApInterface();
  this->setApKeepAlive(false);
  this->_apGraceShutdownTime = 0;
  WiFi.mode((this->_state == OnLine || this->_state == Connecting) ? WIFI_STA : WIFI_OFF);
}

////////////////////////////////////////////////////////////////////

void IotWebConf::blink(unsigned long repeatMs, byte dutyCyclePercent)
{
  if (repeatMs == 0)
  {
    this->stopCustomBlink();
  }
  else
  {
    this->_blinkOnMs = repeatMs * dutyCyclePercent / 100;
    this->_blinkOffMs = repeatMs * (100 - dutyCyclePercent) / 100;
  }
}

void IotWebConf::fineBlink(unsigned long onMs, unsigned long offMs)
{
  this->_blinkOnMs = onMs;
  this->_blinkOffMs = offMs;
}

void IotWebConf::stopCustomBlink()
{
  this->_blinkOnMs = this->_internalBlinkOnMs;
  this->_blinkOffMs = this->_internalBlinkOffMs;
}

void IotWebConf::blinkInternal(unsigned long repeatMs, unsigned long onMs)
{
  this->fineBlink(onMs, repeatMs - onMs);
  this->_internalBlinkOnMs = this->_blinkOnMs;
  this->_internalBlinkOffMs = this->_blinkOffMs;
}

void IotWebConf::doBlink()
{
  if (IOTWEBCONF_STATUS_ENABLED)
  {
    unsigned long now = millis();
    unsigned long delayMs =
      this->_blinkStateOn ? this->_blinkOnMs : this->_blinkOffMs;
    if (delayMs < now - this->_lastBlinkTime)
    {
      this->_blinkStateOn = !this->_blinkStateOn;
      this->_lastBlinkTime = now;
      digitalWrite(this->_statusPin, this->_blinkStateOn ? this->_statusOnLevel : !this->_statusOnLevel);
    }
  }
}

void IotWebConf::forceApMode(bool doForce)
{
  if (this->_forceApMode == doForce)
  {
     // Already in the requested mode;
    return;
  }

  this->_forceApMode = doForce;
  if (doForce)
  {
    if ((this->_state == Connecting) || (this->_state == OnLine))
    {
      // Bring the AP up alongside STA without a state change, so an online device isn't dropped
      // offline just to show the config portal. WiFi.mode() must be set to AP_STA *before*
      // setupAp() runs -- setupAp() no longer forces its own mode, precisely so this ordering
      // works (see setupAp()).
      IOTWEBCONF_DEBUG_LINE(F("Start forcing AP mode (STA stays up)"));
      WiFi.mode(WIFI_AP_STA);
      this->setupAp();
      this->setApKeepAlive(true);
    }
    else if (this->_state != ApMode)
    {
      IOTWEBCONF_DEBUG_LINE(F("Start forcing AP mode"));
      this->changeState(ApMode);
    }
  }
  else
  {
    if ((this->_state == Connecting) || (this->_state == OnLine))
    {
      if (this->_apActiveBeforeConnect)
      {
        // The AP here is up only because of the force (see the doForce branch above) -- tear it
        // down directly, no changeState() needed since _state never changed to bring it up.
        IOTWEBCONF_DEBUG_LINE(F("Stopping AP mode force (STA stays up)."));
        this->stopApInterface();
        WiFi.mode(WIFI_STA);
        this->setApKeepAlive(false);
        this->_apGraceShutdownTime = 0;
      }
    }
    else if ((this->_state == ApMode) || (this->_state == NotConfigured))
    {
      if (this->mustStayInApMode())
      {
        IOTWEBCONF_DEBUG_LINE(F("Requested stopping to force AP mode, but we cannot leave the AP mode now."));
      }
      else
      {
        IOTWEBCONF_DEBUG_LINE(F("Stopping AP mode force."));
        // Leaving Ap/NotConfigured via an explicit forceApMode(false) should NOT keep the AP
        // alive across the ensuing connection attempt (unlike checkApTimeout()'s natural,
        // unforced timeout, which should) -- veto the keep-alive stateChanged() would otherwise
        // derive from oldState==ApMode/NotConfigured.
        this->_suppressNextApKeepAlive = true;
        this->changeState(Connecting);
      }
    }
  }
}

bool IotWebConf::connectAp(const char* apName, const char* password)
{
  return WiFi.softAP(apName, password);
}
void IotWebConf::connectWifi(const char* ssid, const char* password)
{
  WiFi.begin(ssid, password);
}
WifiAuthInfo* IotWebConf::handleConnectWifiFailure()
{
  return nullptr;
}

void IotWebConf::forceWifiConnection()
{
  this->_wifiAuthInfo = {this->_wifiParameters._wifiSsid, this->_wifiParameters._wifiPassword};
  this->changeState(Connecting);
}

} // end namespace
