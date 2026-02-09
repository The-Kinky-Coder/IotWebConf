#ifndef IotWebConfIpAddress_h
#define IotWebConfIpAddress_h

#include <Arduino.h>
#include <stdint.h>
#include <string.h>

namespace iotwebconf
{

class IpAddress
{
public:
  enum AddressType : uint8_t
  {
    None = 0,
    V4,
    V6
  };

  IpAddress();

  static IpAddress parse(const char* text);
  static IpAddress parse(const String& text) { return parse(text.c_str()); }
  static IpAddress fromString(const char* text) { return parse(text); }
  static IpAddress fromString(const String& text) { return parse(text); }

  bool isV4() const { return this->_type == V4; }
  bool isV6() const { return this->_type == V6; }
  bool isSet() const { return this->_type != None; }

  String toString(bool bracketIpv6 = false) const;

  bool operator==(const IpAddress& other) const;
  bool operator!=(const IpAddress& other) const { return !(*this == other); }

private:
  AddressType _type = None;
  uint8_t _bytes[16];

  static bool parseIpv4(const char* text, uint8_t* out);
  static bool parseIpv6(const char* text, uint8_t* out);
  static bool parseHextets(const char* text, size_t length, uint16_t* out, int& count);
};

} // namespace iotwebconf

#endif
