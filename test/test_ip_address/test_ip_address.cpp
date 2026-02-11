#include <Arduino.h>
#include <unity.h>

#include <IotWebConfIpAddress.h>

using iotwebconf::IpAddress;

void test_ipv4_parse_and_string()
{
  IpAddress address = IpAddress::parse("192.168.1.10");
  TEST_ASSERT_TRUE(address.isSet());
  TEST_ASSERT_TRUE(address.isV4());
  TEST_ASSERT_FALSE(address.isV6());
  TEST_ASSERT_EQUAL_STRING("192.168.1.10", address.toString().c_str());
}

void test_ipv4_invalid()
{
  IpAddress address = IpAddress::parse("300.1.2.3");
  TEST_ASSERT_FALSE(address.isSet());
  TEST_ASSERT_FALSE(address.isV4());
  TEST_ASSERT_FALSE(address.isV6());
}

void test_ipv6_parse_full()
{
  IpAddress address = IpAddress::parse("2001:0db8:0000:0000:0000:ff00:0042:8329");
  TEST_ASSERT_TRUE(address.isSet());
  TEST_ASSERT_TRUE(address.isV6());
  TEST_ASSERT_FALSE(address.isV4());

  IpAddress roundTrip = IpAddress::parse(address.toString());
  TEST_ASSERT_TRUE(roundTrip.isV6());
  TEST_ASSERT_TRUE(address == roundTrip);
}

void test_ipv6_parse_compressed()
{
  IpAddress address = IpAddress::parse("2001:db8::1");
  TEST_ASSERT_TRUE(address.isSet());
  TEST_ASSERT_TRUE(address.isV6());

  IpAddress roundTrip = IpAddress::parse(address.toString());
  TEST_ASSERT_TRUE(roundTrip.isV6());
  TEST_ASSERT_TRUE(address == roundTrip);
}

void test_ipv6_bracketed()
{
  IpAddress address = IpAddress::parse("[2001:db8::abcd]");
  TEST_ASSERT_TRUE(address.isSet());
  TEST_ASSERT_TRUE(address.isV6());

  String bracketed = address.toString(true);
  TEST_ASSERT_TRUE(bracketed.startsWith("["));
  TEST_ASSERT_TRUE(bracketed.endsWith("]"));
  IpAddress roundTrip = IpAddress::parse(bracketed);
  TEST_ASSERT_TRUE(roundTrip.isV6());
  TEST_ASSERT_TRUE(address == roundTrip);
}

void test_equality()
{
  IpAddress first = IpAddress::parse("10.0.0.1");
  IpAddress second = IpAddress::parse("10.0.0.1");
  IpAddress third = IpAddress::parse("10.0.0.2");
  TEST_ASSERT_TRUE(first == second);
  TEST_ASSERT_TRUE(first != third);
}

void test_ipv6_unspecified()
{
  IpAddress address = IpAddress::parse("::");
  TEST_ASSERT_TRUE(address.isSet());
  TEST_ASSERT_TRUE(address.isV6());
  TEST_ASSERT_TRUE(address.isUnspecified());
  
  // Regular IPv6 addresses should not be unspecified
  IpAddress regularV6 = IpAddress::parse("2001:db8::1");
  TEST_ASSERT_TRUE(regularV6.isSet());
  TEST_ASSERT_TRUE(regularV6.isV6());
  TEST_ASSERT_FALSE(regularV6.isUnspecified());
}

void test_ipv4_unspecified()
{
  IpAddress address = IpAddress::parse("0.0.0.0");
  TEST_ASSERT_TRUE(address.isSet());
  TEST_ASSERT_TRUE(address.isV4());
  TEST_ASSERT_TRUE(address.isUnspecified());
  
  // Regular IPv4 addresses should not be unspecified
  IpAddress regularV4 = IpAddress::parse("192.168.1.1");
  TEST_ASSERT_TRUE(regularV4.isSet());
  TEST_ASSERT_TRUE(regularV4.isV4());
  TEST_ASSERT_FALSE(regularV4.isUnspecified());
}

void setup()
{
  UNITY_BEGIN();
  RUN_TEST(test_ipv4_parse_and_string);
  RUN_TEST(test_ipv4_invalid);
  RUN_TEST(test_ipv6_parse_full);
  RUN_TEST(test_ipv6_parse_compressed);
  RUN_TEST(test_ipv6_bracketed);
  RUN_TEST(test_equality);
  RUN_TEST(test_ipv6_unspecified);
  RUN_TEST(test_ipv4_unspecified);
  UNITY_END();
}

void loop()
{
}
