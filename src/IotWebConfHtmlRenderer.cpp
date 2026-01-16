/**
 * IotWebConfHtmlRenderer.cpp -- IotWebConf is an ESP8266/ESP32
 *   non blocking WiFi/AP web configuration library for Arduino.
 *   https://github.com/prampec/IotWebConf
 *
 * Copyright (C) 2020 Balazs Kelemen <prampec+arduino@gmail.com>
 *
 * This software may be modified and distributed under the terms
 * of the MIT license.  See the LICENSE file for details.
 */

#include "IotWebConfHtmlRenderer.h"
#include "IotWebConf.h"
#include "IotWebConfParameter.h"
#include "IotWebConfOptionalGroup.h"

namespace iotwebconf
{

///////////////////////////////////////////////////////////////////////////////
// Standard HTML Templates
// Moved from IotWebConfParameter.h

const char IOTWEBCONF_HTML_FORM_GROUP_START[] PROGMEM =
  "<fieldset id='{i}'><legend>{b}</legend>\n";
const char IOTWEBCONF_HTML_FORM_GROUP_END[] PROGMEM =
  "</fieldset>\n";

const char IOTWEBCONF_HTML_FORM_PARAM[] PROGMEM =
  "<div class='{s}'><label for='{i}'>{b}</label><input type='{t}' id='{i}' "
  "name='{i}' maxlength='{l}' placeholder='{p}' value='{v}' {c}/>"
  "<div class='em'>{e}</div></div>\n";

const char IOTWEBCONF_HTML_FORM_SELECT_PARAM[] PROGMEM =
  "<div class='{s}'><label for='{i}'>{b}</label><select id='{i}' "
  "name='{i}' {c}/>\n{o}"
  "</select><div class='em'>{e}</div></div>\n";
const char IOTWEBCONF_HTML_FORM_OPTION[] PROGMEM =
  "<option value='{v}'{s}>{n}</option>\n";


///////////////////////////////////////////////////////////////////////////////
// StandardHtmlRenderer Implementation

void StandardHtmlRenderer::render(IotWebConf* iotWebConf, WebRequestWrapper* webRequestWrapper, bool dataArrived)
{
    // -- Display config portal
    IOTWEBCONF_DEBUG_LINE(F("Configuration page requested."));

    // Send chunked output instead of one String, to avoid
    // filling memory if using many parameters.
    webRequestWrapper->sendHeader(
        "Cache-Control", "no-cache, no-store, must-revalidate");
    webRequestWrapper->sendHeader("Pragma", "no-cache");
    webRequestWrapper->sendHeader("Expires", "-1");
    webRequestWrapper->setContentLength(CONTENT_LENGTH_UNKNOWN);
    webRequestWrapper->send(200, "text/html; charset=UTF-8", "");

    HtmlFormatProvider* htmlFormatProvider = iotWebConf->getHtmlFormatProvider();

    String content = htmlFormatProvider->getHead();
    content.replace("{v}", "Config ESP");
    content += htmlFormatProvider->getScript();
    content += htmlFormatProvider->getStyle();
    content += htmlFormatProvider->getHeadExtension();
    content += htmlFormatProvider->getHeadEnd();

    content += htmlFormatProvider->getFormStart();

    webRequestWrapper->sendContent(content);

#ifdef IOTWEBCONF_DEBUG_TO_SERIAL
    Serial.println("Rendering parameters:");
    iotWebConf->getSystemParameterGroup()->debugTo(&Serial);
    iotWebConf->getCustomParameterGroup()->debugTo(&Serial);
#endif
    // -- Add parameters to the form
    this->render(iotWebConf->getSystemParameterGroup(), dataArrived, webRequestWrapper);
    this->render(iotWebConf->getCustomParameterGroup(), dataArrived, webRequestWrapper);

    content = htmlFormatProvider->getFormEnd();

    if (iotWebConf->getUpdatePath() != nullptr)
    {
      String pitem = htmlFormatProvider->getUpdate();
      pitem.replace("{u}", iotWebConf->getUpdatePath());
      content += pitem;
    }

    // -- Fill config version string;
    {
      String pitem = htmlFormatProvider->getConfigVer();
      pitem.replace("{v}", iotWebConf->getConfigVersion());
      content += pitem;
    }

    content += htmlFormatProvider->getEnd();

    webRequestWrapper->sendContent(content);
    webRequestWrapper->sendContent(F(""));
    webRequestWrapper->stop();
}

void StandardHtmlRenderer::renderConfigSaved(IotWebConf* iotWebConf, WebRequestWrapper* webRequestWrapper)
{
    HtmlFormatProvider* htmlFormatProvider = iotWebConf->getHtmlFormatProvider();

    String page = htmlFormatProvider->getHead();
    page.replace("{v}", "Config ESP");
    page += htmlFormatProvider->getScript();
    page += htmlFormatProvider->getStyle();
//    page += _customHeadElement;
    page += htmlFormatProvider->getHeadExtension();
    page += htmlFormatProvider->getHeadEnd();
    page += "Configuration saved. ";
    
    // We need access to private members of IotWebConf to check AP Password and Wifi SSID
    // or use public getters if available.
    // IotWebConf has getApPasswordParameterValue() and getWifiAuthInfo() or similar?
    // Let's check IotWebConf.h
    
    // IotWebConf has getApPasswordParameterValue()
    // and getWifiSsidParameter() which returns a Parameter*.
    
    // Actually, IotWebConf::getWifiAuthInfo() returns the current auth info, but that might be empty if not set.
    // We should check the parameter values.
    
    // Let's rely on IotWebConf having friend StandardHtmlRenderer so we can access everything?
    // No, IotWebConf.h declares friend IotWebConfParameter but not StandardHtmlRenderer (yet).
    // Let's check IotWebConf.h
    
    // IotWebConf.h does NOT declare StandardHtmlRenderer as friend.
    // So we need public methods.
    
    if (iotWebConf->getApPasswordParameterValue()[0] == '\0')
    {
      page += F("You must change the default AP password to continue. Return "
                "to <a href=''>configuration page</a>.");
    }
    else if (((TextParameter*)iotWebConf->getWifiSsidParameter())->valueBuffer[0] == '\0')
    {
      page += F("You must provide the local wifi settings to continue. Return "
                "to <a href=''>configuration page</a>.");
    }
    else if (iotWebConf->getState() == NotConfigured)
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
}

void StandardHtmlRenderer::render(ConfigItem* item, bool dataArrived, WebRequestWrapper* webRequestWrapper)
{
    // Use the custom type ID system instead of dynamic_cast
    IotWebConfItemType type = item->getType();

    switch(type)
    {
        case IOTWEBCONF_TYPE_GROUP:
            this->renderParameterGroup(static_cast<ParameterGroup*>(item), dataArrived, webRequestWrapper);
            break;
        case IOTWEBCONF_TYPE_OPTIONAL_GROUP:
            this->renderOptionalGroup(static_cast<OptionalParameterGroup*>(item), dataArrived, webRequestWrapper);
            break;
        case IOTWEBCONF_TYPE_PASSWORD:
            this->renderPasswordParameter(static_cast<PasswordParameter*>(item), dataArrived, webRequestWrapper);
            break;
        case IOTWEBCONF_TYPE_CHECKBOX:
            this->renderCheckboxParameter(static_cast<CheckboxParameter*>(item), dataArrived, webRequestWrapper);
            break;
        case IOTWEBCONF_TYPE_SELECT:
            this->renderSelectParameter(static_cast<SelectParameter*>(item), dataArrived, webRequestWrapper);
            break;
        case IOTWEBCONF_TYPE_NUMBER:
            this->renderNumberParameter(static_cast<NumberParameter*>(item), dataArrived, webRequestWrapper);
            break;
        case IOTWEBCONF_TYPE_TEXT:
            this->renderTextParameter(static_cast<TextParameter*>(item), dataArrived, webRequestWrapper);
            break;
        case IOTWEBCONF_TYPE_UNDEFINED:
        default:
            // Fallback or ignore
            break;
    }
}

void StandardHtmlRenderer::renderParameterGroup(ParameterGroup* group, bool dataArrived, WebRequestWrapper* webRequestWrapper)
{
    if (group->label != nullptr)
    {
        String content = FPSTR(IOTWEBCONF_HTML_FORM_GROUP_START);
        content.replace("{b}", group->label);
        content.replace("{i}", group->getId());
        webRequestWrapper->sendContent(content);
    }
    
    ConfigItem* current = group->getFirst();
    while (current != nullptr)
    {
        if (current->visible)
        {
            this->render(current, dataArrived, webRequestWrapper);
        }
        current = current->getNext();
    }

    if (group->label != nullptr)
    {
        String content = FPSTR(IOTWEBCONF_HTML_FORM_GROUP_END);
        content.replace("{b}", group->label);
        content.replace("{i}", group->getId());
        webRequestWrapper->sendContent(content);
    }
}

void StandardHtmlRenderer::renderOptionalGroup(OptionalParameterGroup* group, bool dataArrived, WebRequestWrapper* webRequestWrapper)
{
    if (group->label != nullptr)
    {
      String content = group->getStartTemplate();
      content.replace("{b}", group->label);
      content.replace("{i}", group->getId());
      content.replace("{v}", group->isActive() ? "active" : "inactive");
      if (group->isActive())
      {
        content.replace("{cb}", "hide");
        content.replace("{cf}", "");
      }
      else
      {
        content.replace("{cb}", "");
        content.replace("{cf}", "hide");
      }
      webRequestWrapper->sendContent(content);
    }
    
    // We can access protected members using public getters added to ConfigItem/ParameterGroup
    // or rely on friendship.
    // We updated ParameterGroup to have getFirst() and getNext().
    
    ConfigItem* current = group->getFirst();
    while (current != nullptr)
    {
      if (current->visible)
      {
        this->render(current, dataArrived, webRequestWrapper);
      }
      current = group->getNextItemOf(current);
    }
    
    if (group->label != nullptr)
    {
      String content = group->getEndTemplate();
      content.replace("{b}", group->label);
      content.replace("{i}", group->getId());
      webRequestWrapper->sendContent(content);
    }
}

void StandardHtmlRenderer::renderTextParameter(TextParameter* param, bool dataArrived, WebRequestWrapper* webRequestWrapper)

{
    String content = this->renderTextParameterHtml(
        param,
        "text",
        dataArrived && webRequestWrapper->hasArg(param->getId()),
        dataArrived ? webRequestWrapper->arg(param->getId()) : ""
    );
    webRequestWrapper->sendContent(content);
}

void StandardHtmlRenderer::renderPasswordParameter(PasswordParameter* param, bool dataArrived, WebRequestWrapper* webRequestWrapper)
{
    // Password rendering logic
    String content = this->renderTextParameterHtml(
        param,
        "password",
        true, // Always treat as if we have value (empty) to avoid showing actual password
        ""
    );
    webRequestWrapper->sendContent(content);
}

void StandardHtmlRenderer::renderNumberParameter(NumberParameter* param, bool dataArrived, WebRequestWrapper* webRequestWrapper)
{
    String content = this->renderTextParameterHtml(
        param,
        "number",
        dataArrived && webRequestWrapper->hasArg(param->getId()),
        dataArrived ? webRequestWrapper->arg(param->getId()) : ""
    );
    webRequestWrapper->sendContent(content);
}

void StandardHtmlRenderer::renderCheckboxParameter(CheckboxParameter* param, bool dataArrived, WebRequestWrapper* webRequestWrapper)
{
    bool checkSelected = false;
    bool hasValue = dataArrived && webRequestWrapper->hasArg(param->getId());
    String valueFromPost = hasValue ? webRequestWrapper->arg(param->getId()) : "";

    if (dataArrived)
    {
        if (hasValue && valueFromPost.equals("selected"))
        {
            checkSelected = true;
        }
    }
    else
    {
        if (param->isChecked())
        {
            checkSelected = true;
        }
    }

    const char* originalCustomHtml = param->customHtml;
    
    if (checkSelected)
    {
        param->customHtml = "checked='checked'";
    }
    else
    {
        param->customHtml = nullptr;
    }
    
    String content = this->renderTextParameterHtml(
        param,
        "checkbox",
        true, 
        "selected"
    );
    
    // Restore
    param->customHtml = originalCustomHtml;
    
    webRequestWrapper->sendContent(content);
}

void StandardHtmlRenderer::renderSelectParameter(SelectParameter* param, bool dataArrived, WebRequestWrapper* webRequestWrapper)
{
    bool hasValue = dataArrived && webRequestWrapper->hasArg(param->getId());
    String valueFromPost = hasValue ? webRequestWrapper->arg(param->getId()) : "";
    
    String options = "";

    for (size_t i=0; i<param->_optionCount; i++)
    {
        const char *optionValue = (param->_optionValues + (i*param->getLength()) );
        const char *optionName = (param->_optionNames + (i*param->_nameLength) );
        String oitem = FPSTR(IOTWEBCONF_HTML_FORM_OPTION);
        oitem.replace("{v}", optionValue);
        oitem.replace("{n}", optionName);

        if ((hasValue && (valueFromPost == optionValue)) ||
            (!hasValue && strncmp(param->valueBuffer, optionValue, param->getLength()) == 0))
        {
             oitem.replace("{s}", " selected");
        }
        else
        {
             oitem.replace("{s}", "");
        }

        options += oitem;
    }

    String pitem = FPSTR(IOTWEBCONF_HTML_FORM_SELECT_PARAM);

    pitem.replace("{b}", param->label);
    pitem.replace("{i}", param->getId());
    pitem.replace(
        "{c}", param->customHtml == nullptr ? "" : param->customHtml);
    pitem.replace(
        "{s}",
        param->errorMessage == nullptr ? "" : "de"); // Div style class.
    pitem.replace(
        "{e}",
        param->errorMessage == nullptr ? "" : param->errorMessage);
    pitem.replace("{o}", options);

    webRequestWrapper->sendContent(pitem);
}

String StandardHtmlRenderer::renderTextParameterHtml(
    TextParameter* param, 
    const char* type, 
    bool hasValueFromPost, 
    String valueFromPost)
{
    char parLength[12];
    String pitem = FPSTR(IOTWEBCONF_HTML_FORM_PARAM);

    pitem.replace("{b}", param->label);
    pitem.replace("{t}", type);
    pitem.replace("{i}", param->getId());
    pitem.replace("{p}", param->placeholder == nullptr ? "" : param->placeholder);
    
    snprintf(parLength, 12, "%d", param->getLength()-1);
    pitem.replace("{l}", parLength);
    
    if (hasValueFromPost)
    {
        // -- Value from previous submit
        pitem.replace("{v}", valueFromPost);
    }
    else
    {
        // -- Value from config
        pitem.replace("{v}", param->valueBuffer);
    }
    pitem.replace(
        "{c}", param->customHtml == nullptr ? "" : param->customHtml);
    pitem.replace(
        "{s}",
        param->errorMessage == nullptr ? "" : "de"); // Div style class.
    pitem.replace(
        "{e}",
        param->errorMessage == nullptr ? "" : param->errorMessage);

    return pitem;
}

} // namespace iotwebconf
