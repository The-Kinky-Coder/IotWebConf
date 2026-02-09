#include "IotWebConfIpAddress.h"

#if defined(ESP32) || defined(ESP8266)
#include <lwip/inet.h>
#include <lwip/ip_addr.h>
#endif

namespace iotwebconf
{

IpAddress::IpAddress() : _type(None)
{
  memset(this->_bytes, 0, sizeof(this->_bytes));
}

IpAddress IpAddress::parse(const char* text)
{
  IpAddress result;
  if (text == nullptr)
  {
    return result;
  }

  String input = String(text);
  input.trim();
  if (input.length() == 0)
  {
    return result;
  }

  bool hasBrackets = input.startsWith("[") && input.endsWith("]");
  if (hasBrackets)
  {
    input = input.substring(1, input.length() - 1);
  }

  uint8_t bytes[16];
  if (parseIpv4(input.c_str(), bytes))
  {
    result._type = V4;
    memcpy(result._bytes, bytes, 4);
    return result;
  }

  if (parseIpv6(input.c_str(), bytes))
  {
    result._type = V6;
    memcpy(result._bytes, bytes, 16);
    return result;
  }

  return result;
}

String IpAddress::toString(bool bracketIpv6) const
{
  if (this->_type == None)
  {
    return String();
  }

  if (this->_type == V4)
  {
#if defined(ESP32) || defined(ESP8266)
    char buffer[16] = {0};
    ip_addr_t addr;
    memset(&addr, 0, sizeof(addr));
    addr.type = IPADDR_TYPE_V4;
    addr.u_addr.ip4.addr = PP_HTONL(
      (static_cast<uint32_t>(this->_bytes[0]) << 24)
      | (static_cast<uint32_t>(this->_bytes[1]) << 16)
      | (static_cast<uint32_t>(this->_bytes[2]) << 8)
      | static_cast<uint32_t>(this->_bytes[3]));
    if (ipaddr_ntoa_r(&addr, buffer, sizeof(buffer)) != nullptr)
    {
      return String(buffer);
    }
#endif
    return String(this->_bytes[0]) + "." +
      String(this->_bytes[1]) + "." +
      String(this->_bytes[2]) + "." +
      String(this->_bytes[3]);
  }

  String formatted;
#if defined(ESP32) || defined(ESP8266)
  char buffer[40] = {0};
  ip_addr_t addr;
  memset(&addr, 0, sizeof(addr));
  addr.type = IPADDR_TYPE_V6;
  memcpy(addr.u_addr.ip6.addr, this->_bytes, 16);
  const char* printed = ipaddr_ntoa_r(&addr, buffer, sizeof(buffer));
  if (printed != nullptr)
  {
    formatted = String(printed);
  }
#endif

  if (formatted.length() == 0)
  {
    formatted.reserve(48);
    for (int i = 0; i < 8; ++i)
    {
      if (i > 0)
      {
        formatted += ":";
      }
      uint16_t value = (static_cast<uint16_t>(this->_bytes[i * 2]) << 8)
        | static_cast<uint16_t>(this->_bytes[i * 2 + 1]);
      formatted += String(value, HEX);
    }
  }

  if (bracketIpv6)
  {
    return String("[") + formatted + "]";
  }
  return formatted;
}

bool IpAddress::operator==(const IpAddress& other) const
{
  if (this->_type != other._type)
  {
    return false;
  }
  if (this->_type == None)
  {
    return true;
  }
  size_t length = this->_type == V4 ? 4 : 16;
  return memcmp(this->_bytes, other._bytes, length) == 0;
}

bool IpAddress::parseIpv4(const char* text, uint8_t* out)
{
  if (text == nullptr || out == nullptr)
  {
    return false;
  }

  int parts[4] = {0};
  int partIndex = 0;
  int value = 0;
  bool hasDigit = false;

  for (const char* cursor = text; *cursor != '\0'; ++cursor)
  {
    char ch = *cursor;
    if (ch >= '0' && ch <= '9')
    {
      value = value * 10 + (ch - '0');
      if (value > 255)
      {
        return false;
      }
      hasDigit = true;
    }
    else if (ch == '.')
    {
      if (!hasDigit || partIndex >= 3)
      {
        return false;
      }
      parts[partIndex++] = value;
      value = 0;
      hasDigit = false;
    }
    else
    {
      return false;
    }
  }

  if (!hasDigit || partIndex != 3)
  {
    return false;
  }
  parts[partIndex] = value;

  for (int i = 0; i < 4; ++i)
  {
    out[i] = static_cast<uint8_t>(parts[i]);
  }
  return true;
}

bool IpAddress::parseHextets(const char* text, size_t length, uint16_t* out, int& count)
{
  if (text == nullptr || out == nullptr)
  {
    return false;
  }

  count = 0;
  size_t index = 0;
  while (index < length)
  {
    if (count >= 8)
    {
      return false;
    }

    uint32_t value = 0;
    int digits = 0;
    while (index < length)
    {
      char ch = text[index];
      if (ch == ':')
      {
        break;
      }
      uint8_t nibble;
      if (ch >= '0' && ch <= '9')
      {
        nibble = static_cast<uint8_t>(ch - '0');
      }
      else if (ch >= 'a' && ch <= 'f')
      {
        nibble = static_cast<uint8_t>(ch - 'a' + 10);
      }
      else if (ch >= 'A' && ch <= 'F')
      {
        nibble = static_cast<uint8_t>(ch - 'A' + 10);
      }
      else
      {
        return false;
      }
      value = (value << 4) | nibble;
      if (value > 0xFFFF)
      {
        return false;
      }
      ++digits;
      ++index;
    }

    if (digits == 0)
    {
      return false;
    }

    out[count++] = static_cast<uint16_t>(value);

    if (index < length)
    {
      if (text[index] != ':')
      {
        return false;
      }
      ++index;
      if (index == length)
      {
        return false;
      }
    }
  }

  return true;
}

bool IpAddress::parseIpv6(const char* text, uint8_t* out)
{
  if (text == nullptr || out == nullptr)
  {
    return false;
  }

#if defined(ESP32) || defined(ESP8266)
  ip_addr_t addr;
  if (ipaddr_aton(text, &addr) != 0)
  {
#if defined(IP_IS_V6_VAL)
    if (IP_IS_V6_VAL(addr))
    {
      memcpy(out, addr.u_addr.ip6.addr, 16);
      return true;
    }
#endif
  }
#endif

  String input = String(text);
  int doubleColon = input.indexOf("::");
  if (doubleColon >= 0)
  {
    if (input.indexOf("::", doubleColon + 2) >= 0)
    {
      return false;
    }
  }

  uint16_t left[8];
  uint16_t right[8];
  int leftCount = 0;
  int rightCount = 0;

  if (doubleColon >= 0)
  {
    String leftPart = input.substring(0, doubleColon);
    String rightPart = input.substring(doubleColon + 2);

    if (leftPart.length() > 0)
    {
      if (!parseHextets(leftPart.c_str(), leftPart.length(), left, leftCount))
      {
        return false;
      }
    }

    if (rightPart.length() > 0)
    {
      if (!parseHextets(rightPart.c_str(), rightPart.length(), right, rightCount))
      {
        return false;
      }
    }

    int total = leftCount + rightCount;
    if (total > 8)
    {
      return false;
    }

    int fillCount = 8 - total;
    for (int i = 0; i < leftCount; ++i)
    {
      out[i * 2] = static_cast<uint8_t>(left[i] >> 8);
      out[i * 2 + 1] = static_cast<uint8_t>(left[i] & 0xFF);
    }
    for (int i = 0; i < fillCount; ++i)
    {
      int index = (leftCount + i) * 2;
      out[index] = 0;
      out[index + 1] = 0;
    }
    for (int i = 0; i < rightCount; ++i)
    {
      int index = (leftCount + fillCount + i) * 2;
      out[index] = static_cast<uint8_t>(right[i] >> 8);
      out[index + 1] = static_cast<uint8_t>(right[i] & 0xFF);
    }
    return true;
  }

  if (!parseHextets(input.c_str(), input.length(), left, leftCount))
  {
    return false;
  }
  if (leftCount != 8)
  {
    return false;
  }
  for (int i = 0; i < 8; ++i)
  {
    out[i * 2] = static_cast<uint8_t>(left[i] >> 8);
    out[i * 2 + 1] = static_cast<uint8_t>(left[i] & 0xFF);
  }
  return true;
}

} // namespace iotwebconf
