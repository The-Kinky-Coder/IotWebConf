/**
 * IotWebConfSettings.h -- IotWebConf is an ESP8266/ESP32
 *   non blocking WiFi/AP web configuration library for Arduino.
 *   https://github.com/prampec/IotWebConf
 *
 * Copyright (C) 2020 Balazs Kelemen <prampec+arduino@gmail.com>
 *
 * This software may be modified and distributed under the terms
 * of the MIT license.  See the LICENSE file for details.
 */

#ifndef IotWebConfSettings_h
#define IotWebConfSettings_h

// -- We might want to place the config in the EEPROM in an offset.
#ifndef IOTWEBCONF_CONFIG_START
# define IOTWEBCONF_CONFIG_START 0
#endif

// -- Maximal length of any string used in IotWebConfig configuration (e.g.
// ssid).
#ifndef IOTWEBCONF_WORD_LEN
# define IOTWEBCONF_WORD_LEN 33
#endif
// -- Maximal length of password used in IotWebConfig configuration.
#ifndef IOTWEBCONF_PASSWORD_LEN
# define IOTWEBCONF_PASSWORD_LEN 33
#endif

// -- IotWebConf tries to connect to the local network for an amount of time
// before falling back to AP mode.
#ifndef IOTWEBCONF_DEFAULT_WIFI_CONNECTION_TIMEOUT_MS
# define IOTWEBCONF_DEFAULT_WIFI_CONNECTION_TIMEOUT_MS 30000
#endif

// -- Number of retry attempts for transient WiFi connection failures
// (e.g. WL_IDLE_STATUS, WL_DISCONNECTED) before giving up.
#ifndef IOTWEBCONF_WIFI_RETRY_COUNT
# define IOTWEBCONF_WIFI_RETRY_COUNT 3
#endif

// -- Delay in milliseconds between WiFi retry attempts.
#ifndef IOTWEBCONF_WIFI_RETRY_DELAY_MS
# define IOTWEBCONF_WIFI_RETRY_DELAY_MS 5000
#endif

// -- Thing will stay in AP mode for an amount of time on boot, before retrying
// to connect to a WiFi network.
#ifndef IOTWEBCONF_DEFAULT_AP_MODE_TIMEOUT_SECS
# define IOTWEBCONF_DEFAULT_AP_MODE_TIMEOUT_SECS "30"
#endif

// -- Auto-reconnect from AP mode when configured network becomes available.
// Handles power-cut scenarios where router boots slower than device.
#ifndef IOTWEBCONF_AP_AUTO_RECONNECT_ENABLED
# define IOTWEBCONF_AP_AUTO_RECONNECT_ENABLED true
#endif

// -- Minimum interval between auto-reconnect attempts from AP mode.
// Prevents rapid bouncing between AP and STA modes.
#ifndef IOTWEBCONF_AP_AUTO_RECONNECT_INTERVAL_MS
# define IOTWEBCONF_AP_AUTO_RECONNECT_INTERVAL_MS 120000
#endif

// -- Auto-connect to WiFi on boot when credentials are set.
// Set to 1 to start in STA mode (unless AP is forced by config pin).
#ifndef IOTWEBCONF_DEFAULT_AUTO_CONNECT
# define IOTWEBCONF_DEFAULT_AUTO_CONNECT 0
#endif

// -- mDNS should allow you to connect to this device with a hostname provided
// by the device. E.g. mything.local
// (This is not very likely to work, and MDNS is not very well documented.)
#ifndef IOTWEBCONF_CONFIG_DONT_USE_MDNS
# define IOTWEBCONF_CONFIG_USE_MDNS 80
#endif

// -- Logs progress information to Serial if enabled.
#ifndef IOTWEBCONF_DEBUG_DISABLED
# define IOTWEBCONF_DEBUG_TO_SERIAL
#endif

// -- Logs passwords to Serial if enabled.
//#define IOTWEBCONF_DEBUG_PWD_TO_SERIAL

// -- Helper define for serial debug
#ifdef IOTWEBCONF_DEBUG_TO_SERIAL
# define IOTWEBCONF_DEBUG_LINE(MSG) Serial.println(MSG)
#else
# define IOTWEBCONF_DEBUG_LINE(MSG)
#endif

// -- EEPROM config starts with a special prefix of length defined here.
#ifndef IOTWEBCONF_CONFIG_VERSION_LENGTH
# define IOTWEBCONF_CONFIG_VERSION_LENGTH 4
#endif

#ifndef IOTWEBCONF_DNS_PORT
# define IOTWEBCONF_DNS_PORT 53
#endif

#endif