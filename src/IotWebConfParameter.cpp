/**
 * IotWebConfParameter.cpp -- IotWebConf is an ESP8266/ESP32
 *   non blocking WiFi/AP web configuration library for Arduino.
 *   https://github.com/prampec/IotWebConf
 *
 * Copyright (C) 2020 Balazs Kelemen <prampec+arduino@gmail.com>
 *
 * This software may be modified and distributed under the terms
 * of the MIT license.  See the LICENSE file for details.
 */

#include "IotWebConfParameter.h"
#include "IotWebConfHtmlRenderer.h" // Needed for StandardHtmlRenderer

namespace iotwebconf
{

// ---------------------------------------------------------
// Global default renderer instance
// ---------------------------------------------------------
// This is a static instance of the standard renderer.
// It allows us to maintain backward compatibility where
// parameters render themselves using a default behavior.
static StandardHtmlRenderer _defaultRenderer;

// ---------------------------------------------------------
// ParameterGroup
// ---------------------------------------------------------

ParameterGroup::ParameterGroup(
  const char* id, const char* label) :
  ConfigItem(id)
{
  this->label = label;
}

void ParameterGroup::addItem(ConfigItem* configItem)
{
  if (configItem->_parentItem != nullptr)
  {
    return; // Item must not be added two times.
  }
  if (this->_firstItem == nullptr)
  {
    this->_firstItem = configItem;
    return;
  }
  ConfigItem* current = this->_firstItem;
  while (current->_nextItem != nullptr)
  {
    current = current->_nextItem;
  }
  current->_nextItem = configItem;
  configItem->_parentItem = this;
}

int ParameterGroup::getStorageSize()
{
  int size = 0;
  ConfigItem* current = this->_firstItem;
  while (current != nullptr)
  {
    size += current->getStorageSize();
    current = current->_nextItem;
  }
  return size;
}
void ParameterGroup::applyDefaultValue()
{
  ConfigItem* current = this->_firstItem;
  while (current != nullptr)
  {
    current->applyDefaultValue();
    current = current->_nextItem;
  }
}

void ParameterGroup::storeValue(
  std::function<void(SerializationData* serializationData)> doStore)
{
  ConfigItem* current = this->_firstItem;
  while (current != nullptr)
  {
    current->storeValue(doStore);
    current = current->_nextItem;
  }
}
void ParameterGroup::loadValue(
  std::function<void(SerializationData* serializationData)> doLoad)
{
  ConfigItem* current = this->_firstItem;
  while (current != nullptr)
  {
    current->loadValue(doLoad);
    current = current->_nextItem;
  }
}

void ParameterGroup::update(WebRequestWrapper* webRequestWrapper)
{
  ConfigItem* current = this->_firstItem;
  while (current != nullptr)
  {
    current->update(webRequestWrapper);
    current = current->_nextItem;
  }
}
void ParameterGroup::clearErrorMessage()
{
  ConfigItem* current = this->_firstItem;
  while (current != nullptr)
  {
    current->clearErrorMessage();
    current = current->_nextItem;
  }
}
void ParameterGroup::debugTo(Stream* out)
{
  out->print('[');
  out->print(this->getId());
  out->println(']');

  // -- Here is some overcomplicated logic to have nice debug output.
  bool ownItem = false;
  bool lastItem = false;
  PrefixStreamWrapper stream =
    PrefixStreamWrapper(
      out,
      [&](Stream* out1)
    {
      if (ownItem)
      {
        ownItem = false;
        return (size_t)0;
      }
      if (lastItem)
      {
        return out1->print("    ");
      }
      else
      {
        return out1->print("|   ");
      }
    });

  ConfigItem* current = this->_firstItem;
  while (current != nullptr)
  {
    if (current->_nextItem == nullptr)
    {
      out->print("\\-- ");
    }
    else
    {
      out->print("|-- ");
    }
    ownItem = true;
    lastItem = (current->_nextItem == nullptr);
    current->debugTo(&stream);
    current = current->_nextItem;
  }
}

#ifdef IOTWEBCONF_ENABLE_JSON
void ParameterGroup::loadFromJson(JsonObject jsonObject)
{
  if (!jsonObject[this->getId()].isNull())
  {
    JsonObject myObject = jsonObject[this->getId()].as<JsonObject>();
    ConfigItem* current = this->_firstItem;
    while (current != nullptr)
    {
      current->loadFromJson(myObject);
      current = current->_nextItem;
    }
  }
}
void ParameterGroup::saveToJson(JsonObject& jsonObject)
{
  JsonObject myObject = jsonObject[this->getId()].to<JsonObject>();
  ConfigItem* current = this->_firstItem;
  while (current != nullptr)
  {
    current->saveToJson(myObject);
    current = current->_nextItem;
  }
}
#endif

// ---------------------------------------------------------
// ConfigItem (Base)
// ---------------------------------------------------------

void ConfigItem::renderHtml(bool dataArrived, WebRequestWrapper* webRequestWrapper)
{
    // Default implementation uses the standard renderer
    _defaultRenderer.render(this, dataArrived, webRequestWrapper);
}

// ---------------------------------------------------------
// Parameter (Base)
// ---------------------------------------------------------

Parameter::Parameter(
  const char* label, const char* id, char* valueBuffer, int length,
  const char* defaultValue) :
  ConfigItem(id)
{
  this->label = label;
  this->valueBuffer = valueBuffer;
  this->_length = length;
  this->defaultValue = defaultValue;

  this->errorMessage = nullptr;
}

int Parameter::getStorageSize()
{
  return this->_length;
}

void Parameter::applyDefaultValue()
{
  if (defaultValue != nullptr)
  {
    strncpy(this->valueBuffer, this->defaultValue, this->getLength());
  }
  else
  {
    this->valueBuffer[0] = '\0';
  }
}

void Parameter::storeValue(
  std::function<void(SerializationData* serializationData)> doStore)
{
  SerializationData serializationData;
  serializationData.length = this->_length;
  serializationData.data = (byte*)this->valueBuffer;
  doStore(&serializationData);
}

void Parameter::loadValue(
  std::function<void(SerializationData* serializationData)> doLoad)
{
  SerializationData serializationData;
  serializationData.length = this->_length;
  serializationData.data = (byte*)this->valueBuffer;
  doLoad(&serializationData);
}

void Parameter::update(WebRequestWrapper* webRequestWrapper)
{
  if (webRequestWrapper->hasArg(this->getId()))
  {
    String newValue = webRequestWrapper->arg(this->getId());
    this->update(newValue);
  }
}

void Parameter::clearErrorMessage()
{
    this->errorMessage = nullptr;
}

#ifdef IOTWEBCONF_ENABLE_JSON
void Parameter::loadFromJson(JsonObject jsonObject)
{
  if (!jsonObject[this->getId()].isNull())
  {
#ifdef IOTWEBCONF_DEBUG_TO_SERIAL
    ESP_LOGI(IOTWEBCONF_LOG_TAG, "Applying value from JSON for parameterId: %s", this->getId());
#endif
    const char* value = jsonObject[this->getId()];
    this->update(String(value));
  }
  else
  {
#ifdef IOTWEBCONF_DEBUG_TO_SERIAL
    ESP_LOGI(IOTWEBCONF_LOG_TAG, "No value found in JSON for parameterId: %s", this->getId());
#endif
  }
}

void Parameter::saveToJson(JsonObject& jsonObject)
{
  jsonObject[this->getId()] = this->valueBuffer;
}
#endif


///////////////////////////////////////////////////////////////////////////////

TextParameter::TextParameter(
  const char* label, const char* id, char* valueBuffer, int length,
  const char* defaultValue,
  const char* placeholder,
  const char* customHtml)
  : Parameter(label, id, valueBuffer, length, defaultValue)
{
  this->placeholder = placeholder;
  this->customHtml = customHtml;
}


void TextParameter::update(String newValue)
{
  newValue.toCharArray(this->valueBuffer, this->getLength());
#ifdef IOTWEBCONF_DEBUG_TO_SERIAL
  ESP_LOGI(IOTWEBCONF_LOG_TAG, "%s: %s", this->getId(), this->valueBuffer);
#endif
}

void TextParameter::debugTo(Stream* out)
{
  Parameter* current = this;
  out->print("'");
  out->print(current->getId());
  out->print("' with value: '");
  out->print(current->valueBuffer);
  out->println("'");
}

///////////////////////////////////////////////////////////////////////////////

NumberParameter::NumberParameter(
  const char* label, const char* id, char* valueBuffer, int length,
  const char* defaultValue,
  const char* placeholder,
  const char* customHtml)
  : TextParameter(label, id, valueBuffer, length, defaultValue,
  placeholder, customHtml)
{
}

///////////////////////////////////////////////////////////////////////////////

PasswordParameter::PasswordParameter(
  const char* label, const char* id, char* valueBuffer, int length,
  const char* defaultValue,
  const char* placeholder,
  const char* customHtml)
  : TextParameter(label, id, valueBuffer, length, defaultValue,
  placeholder, customHtml)
{
}

void PasswordParameter::debugTo(Stream* out)
{
  Parameter* current = this;
  out->print("'");
  out->print(current->getId());
  out->print("' with value: ");
#ifdef IOTWEBCONF_DEBUG_PWD_TO_SERIAL
  out->print("'");
  out->print(current->valueBuffer);
  out->println("'");
#else
  out->println(F("<hidden>"));
#endif
}

#ifdef IOTWEBCONF_ENABLE_JSON
// A blank value round-trips safely: update() keeps the stored password when given "".
void PasswordParameter::saveToJson(JsonObject& jsonObject)
{
  jsonObject[this->getId()] = "";
}
#endif

void PasswordParameter::update(String newValue)
{
  Parameter* current = this;
#ifdef IOTWEBCONF_DEBUG_TO_SERIAL
  ESP_LOGI(IOTWEBCONF_LOG_TAG, "Updating parameter: %s", this->getId());
#endif
  if (newValue.length() > 0)
  {
    // -- Value was set.
    newValue.toCharArray(current->valueBuffer, current->getLength());
#ifdef IOTWEBCONF_DEBUG_TO_SERIAL
# ifdef IOTWEBCONF_DEBUG_PWD_TO_SERIAL
    ESP_LOGI(IOTWEBCONF_LOG_TAG, "Password updated: %s", current->valueBuffer);
# else
    ESP_LOGI(IOTWEBCONF_LOG_TAG, "Password <updated>");
# endif
#endif
  }
  else
  {
#ifdef IOTWEBCONF_DEBUG_TO_SERIAL
  ESP_LOGI(IOTWEBCONF_LOG_TAG, "Password <was not changed>");
#endif
  }
}

///////////////////////////////////////////////////////////////////////////////

CheckboxParameter::CheckboxParameter(
    const char* label, const char* id, char* valueBuffer, int length,
    bool defaultValue)
  : TextParameter(label, id, valueBuffer, length, defaultValue ? "selected" : nullptr,
  nullptr, nullptr)
{
}

void CheckboxParameter::update(WebRequestWrapper* webRequestWrapper)
{
  if (webRequestWrapper->hasArg(this->getId()))
  {
    String newValue = webRequestWrapper->arg(this->getId());
    return TextParameter::update(newValue);
  }
  else if (this->visible)
  {
    // HTML will not post back unchecked checkboxes.
    return TextParameter::update("");
  }
}

///////////////////////////////////////////////////////////////////////////////

OptionsParameter::OptionsParameter(
    const char* label, const char* id, char* valueBuffer, int length,
    const char* optionValues, const char* optionNames, size_t optionCount, size_t nameLength,
    const char* defaultValue)
  : TextParameter(label, id, valueBuffer, length, defaultValue,
  nullptr, nullptr)
{
  this->_optionValues = optionValues;
  this->_optionNames = optionNames;
  this->_optionCount = optionCount;
  this->_nameLength = nameLength;
}

///////////////////////////////////////////////////////////////////////////////

SelectParameter::SelectParameter(
    const char* label, const char* id, char* valueBuffer, int length,
    const char* optionValues, const char* optionNames, size_t optionCount, size_t nameLength,
    const char* defaultValue)
  : OptionsParameter(label, id, valueBuffer, length, optionValues, optionNames,
  optionCount, nameLength, defaultValue)
{
}

///////////////////////////////////////////////////////////////////////////////

PrefixStreamWrapper::PrefixStreamWrapper(
  Stream* originalStream,
  std::function<size_t(Stream* stream)> prefixWriter)
{
  this->_originalStream = originalStream;
  this->_prefixWriter = prefixWriter;
}
size_t PrefixStreamWrapper::write(uint8_t data)
{
  size_t sizeOut = checkNewLine();
  sizeOut += this->_originalStream->write(data);
  if (data == 10) // NewLine
  {
    this->_newLine = true;
  }
  return sizeOut;
}
size_t PrefixStreamWrapper::write(const uint8_t *buffer, size_t size)
{
  size_t sizeOut = checkNewLine();
  sizeOut += this->_originalStream->write(buffer, size);
  if (*(buffer + size-1) == 10) // Ends with new line
  {
    this->_newLine = true;
  }
  return sizeOut;
}
size_t PrefixStreamWrapper::checkNewLine()
{
  if (this->_newLine)
  {
    this->_newLine = false;
    return this->_prefixWriter(this->_originalStream);
  }
  return 0;
}

} // end namespace
