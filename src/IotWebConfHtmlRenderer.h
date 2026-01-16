/**
 * IotWebConfHtmlRenderer.h -- IotWebConf is an ESP8266/ESP32
 *   non blocking WiFi/AP web configuration library for Arduino.
 *   https://github.com/prampec/IotWebConf
 *
 * Copyright (C) 2020 Balazs Kelemen <prampec+arduino@gmail.com>
 *
 * This software may be modified and distributed under the terms
 * of the MIT license.  See the LICENSE file for details.
 */

#ifndef IotWebConfHtmlRenderer_h
#define IotWebConfHtmlRenderer_h

#include <Arduino.h>
#include <IotWebConfWebServerWrapper.h>

namespace iotwebconf
{

// Forward declarations to avoid circular dependencies
class IotWebConf;
class ConfigItem;
class ParameterGroup;
class Parameter;
class TextParameter;
class PasswordParameter;
class NumberParameter;
class CheckboxParameter;
class OptionsParameter;
class SelectParameter;
class OptionalParameterGroup;

/**
 * Base class for all HTML renderers.
 * 
 * Uses the Visitor pattern: The renderer "visits" the item to render it.
 * However, since the item structure in IotWebConf is slightly different (Composite pattern),
 * we will pass the item to the renderer method.
 */
class HtmlRenderer
{
public:
  virtual void render(IotWebConf* iotWebConf, WebRequestWrapper* webRequestWrapper, bool dataArrived) = 0;
  virtual void renderConfigSaved(IotWebConf* iotWebConf, WebRequestWrapper* webRequestWrapper) = 0;
  virtual void render(ConfigItem* item, bool dataArrived, WebRequestWrapper* webRequestWrapper) = 0;
};

/**
 * Standard implementation of the HTML renderer.
 * This moves the logic previously found in IotWebConfParameter.cpp
 */
class StandardHtmlRenderer : public HtmlRenderer
{
public:
    void render(IotWebConf* iotWebConf, WebRequestWrapper* webRequestWrapper, bool dataArrived) override;
    void renderConfigSaved(IotWebConf* iotWebConf, WebRequestWrapper* webRequestWrapper) override;
    void render(ConfigItem* item, bool dataArrived, WebRequestWrapper* webRequestWrapper) override;

protected:
    virtual void renderParameterGroup(ParameterGroup* group, bool dataArrived, WebRequestWrapper* webRequestWrapper);
    virtual void renderTextParameter(TextParameter* param, bool dataArrived, WebRequestWrapper* webRequestWrapper);
    virtual void renderPasswordParameter(PasswordParameter* param, bool dataArrived, WebRequestWrapper* webRequestWrapper);
    virtual void renderNumberParameter(NumberParameter* param, bool dataArrived, WebRequestWrapper* webRequestWrapper);
    virtual void renderCheckboxParameter(CheckboxParameter* param, bool dataArrived, WebRequestWrapper* webRequestWrapper);
    virtual void renderSelectParameter(SelectParameter* param, bool dataArrived, WebRequestWrapper* webRequestWrapper);
    virtual void renderOptionalGroup(OptionalParameterGroup* group, bool dataArrived, WebRequestWrapper* webRequestWrapper);
    
    // Helper to generate the HTML string for text-based parameters
    virtual String renderTextParameterHtml(
        TextParameter* param, 
        const char* type, 
        bool hasValueFromPost, 
        String valueFromPost);
};

} // namespace iotwebconf

#endif
