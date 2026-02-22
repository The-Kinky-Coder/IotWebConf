/**
 * IotWebConfMultipleWifi.cpp -- IotWebConf is an ESP8266/ESP32
 *   non blocking WiFi/AP web configuration library for Arduino.
 *   https://github.com/prampec/IotWebConf
 *
 * Copyright (C) 2021 Balazs Kelemen <prampec+arduino@gmail.com>
 *
 * This software may be modified and distributed under the terms
 * of the MIT license.  See the LICENSE file for details.
 */

#include "IotWebConfMultipleWifi.h"

namespace iotwebconf
{

MultipleWifiAddition::MultipleWifiAddition(
  IotWebConf* iotWebConf,
  ChainedWifiParameterGroup sets[],
  size_t setsSize)
{
  this->_iotWebConf = iotWebConf;
  this->_firstSet = &sets[0];
  this->_currentSet = &sets[0];

  ChainedWifiParameterGroup* set = &sets[0];
  for(size_t i=1; i<setsSize; i++)
  {
    set->setNext(&sets[i]);
    set = &sets[i];
  }
}

void MultipleWifiAddition::init()
{
  // -- Add parameter groups.
  ChainedWifiParameterGroup* set = this->_firstSet;
  while(set != nullptr)
  {
    this->_iotWebConf->addSystemParameter(set);
    set = (ChainedWifiParameterGroup*)set->getNext();
  }

  // -- Add custom format provider.
  this->_iotWebConf->setHtmlFormatProvider(
    &this->_optionalGroupHtmlFormatProvider);

  // -- Reset the fallback cursor whenever a connection succeeds, so the next
  // auto-reconnect always starts from the highest-priority slot rather than
  // continuing mid-chain from wherever it last stopped.
  this->_iotWebConf->setWifiConnectionCallback([&]()
    {
      this->_currentSet = this->_firstSet;
      this->_isFallingBack = false;
    });

  // -- Set up handler, that will selects next connection info to use.
  this->_iotWebConf->setFormValidator([&](WebRequestWrapper* webRequestWrapper)
    {
      return this->formValidator(webRequestWrapper);
    });

  // -- Set up handler, that will selects next connection info to use.
  this->_iotWebConf->setWifiConnectionFailedHandler([&]()
    {
        WifiAuthInfo* result;
        while (true)
        {
          if (this->_currentSet == nullptr)
          {
            // Reset the cursor for next time. Do NOT call resetWifiAuthInfo()
            // here — that would restore iwcWifi0 credentials into _wifiAuthInfo,
            // causing an extra wasted attempt at the dead primary network when
            // AP mode times out and re-enters Connecting. Instead, leave
            // _wifiAuthInfo pointing at the last tried credentials; on the next
            // Connecting entry the failure handler will walk from _firstSet.
            this->_currentSet = this->_firstSet;
            result = nullptr;
            break;
          }
          else
          {
            if (this->_currentSet->isActive())
            {
              result = &this->_currentSet->wifiAuthInfo;
              this->_isFallingBack = true; // entering fallback cycle
              this->_currentSet =
                (iotwebconf::ChainedWifiParameterGroup*)this->_currentSet->getNext();
              break;
            }
            else
            {
              this->_currentSet =
                (iotwebconf::ChainedWifiParameterGroup*)this->_currentSet->getNext();
            }
          }
        }
        return result;
    });
};

bool MultipleWifiAddition::formValidator(
  WebRequestWrapper* webRequestWrapper)
{
  ChainedWifiParameterGroup* set = this->_firstSet;
  bool valid = true;

  while(set != nullptr)
  {
    if (set->isActive())
    {
      PasswordParameter* pwdParam = &set->wifiPasswordParameter;
      int l = webRequestWrapper->arg(pwdParam->getId()).length();
      if ((0 < l) && (l < 8))
      {
        pwdParam->errorMessage = "Password length must be at least 8 characters.";
        valid = false;
      }
    }

    set = (ChainedWifiParameterGroup*)set->getNext();
  }

  return valid;
};

}
