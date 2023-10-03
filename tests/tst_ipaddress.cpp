// Copyright (c) 2023 Private Internet Access, Inc.
//
// This file is part of the Private Internet Access Desktop Client.
//
// The Private Internet Access Desktop Client is free software: you can
// redistribute it and/or modify it under the terms of the GNU General Public
// License as published by the Free Software Foundation, either version 3 of
// the License, or (at your option) any later version.
//
// The Private Internet Access Desktop Client is distributed in the hope that
// it will be useful, but WITHOUT ANY WARRANTY; without even the implied
// warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with the Private Internet Access Desktop Client.  If not, see
// <https://www.gnu.org/licenses/>.

#include "src/testresource.h"
#include <QtTest>
#include <kapps_core/src/ipaddress.h>

namespace kapps::core
{

class tst_ipaddress : public QObject
{
    Q_OBJECT

private:
    // Check if first N bytes of two arrays are the same.
    bool bytesUntilIndexEqual(unsigned numberOfBytes, const uint8_t *first, const uint8_t *second)
    {
        return std::equal(first, first + numberOfBytes,  second);
    }

    // Check if bytes from array[startIndex] until end are all zero.
    bool bytesFromIndexToIndexEqualToZero(unsigned startIndex, unsigned endIndex, const uint8_t *array)
    {
        bool result = std::all_of(array + startIndex, array + endIndex, [](uint8_t byteValue)
        {
            return byteValue == 0;
        });

        return result;
    }

private slots:

      void testMaskIpv6()
      {
          // 0/128 (max prefix length)
          {
              Ipv6Address address{};
              Ipv6Address masked = Ipv6Address::maskIpv6(address, 128);
              // 128 is the full length of an ipv6 address - so return the
              // address unchanged
              QVERIFY(address == masked);
          }

          // 2001::1/128 (max prefix length)
          {
              Ipv6Address address{"2001::1"};
              Ipv6Address masked = Ipv6Address::maskIpv6(address, 128);
              // 128 is the full length of an ipv6 address - so return the
              // address unchanged
              QVERIFY(address == masked);
          }

          // 2001:abcd::0/0 (0 prefix length)
          {
              Ipv6Address address("2001:abcd::0");
              Ipv6Address masked = Ipv6Address::maskIpv6(address.address(), 0);
              QVERIFY(masked.isNull());
          }

          // 2001::1/300 (over length prefix - should be clipped to 128)
          {
              Ipv6Address address{"2001::1"};
              Ipv6Address masked = Ipv6Address::maskIpv6(address, 300);
              // 300 is beyond the max length, 128 - we clip it to 128
              // address unchanged
              QVERIFY(address == masked);
          }

          // 2001::f/127
          {
              // :: notation implies all leading zeroes until final 'f' nybble (4 bits),
              Ipv6Address address("2001::f");
              Ipv6Address masked = Ipv6Address::maskIpv6(address, 127);

              // Since the prefix is 127 bits which is 1 less than ipv6 length of 128
              // the mask is the the same as the original address but with the last bit
              // flipped to a 0. Since the original address had 'f' as the last nybble
              // which looks like this in binary: 1111
              // So when we flip the final bit to a 0, it should then look like this: 1110
              // which is 0xe in hex.
              // Assert that the last byte is 0xe
              QVERIFY(masked.address()[15] == 0xe);

              // All other bytes are the same as the original
              QVERIFY(bytesUntilIndexEqual(15, address.address(), masked.address()));
          }

          // 2001:abcd:abcd::abcd/48
          {
              Ipv6Address address("2001:abcd:abcd::abcd");
              Ipv6Address masked = Ipv6Address::maskIpv6(address, 48);

              // 48 / 8 == 6
              // Assert that first 6 bytes are the same, and the rest are 0
              QVERIFY(bytesUntilIndexEqual(6, address.address(), masked.address()));
              //Assert the remainder bytes are all zero
              QVERIFY(bytesFromIndexToIndexEqualToZero(6, sizeof(masked.address()), masked.address()));
          }
          // // 2001:abcd:abcd::abcd/45
          {
              Ipv6Address address("2001:abcd:abcd::abcd");
              Ipv6Address masked = Ipv6Address::maskIpv6(address, 45);

              // 45 / 8 == 5
              // Assert that first 5 bytes are the same
              QVERIFY(bytesUntilIndexEqual(5, address.address(), masked.address()));

              // 6th byte is a partial byte
              // as 45 appears between 40 and 48 (both byte boundaries, multiples of 8)
              // The first 5 bits (45 - 40) in the masked value should match the bits in the
              // original 6th byte, with all the remainder being 0.
              // Since the 6th byte in the original address is "0xcd" in hex
              // which is 11001101 in binary we expect the masked byte to be 11001000 (first 5 bits match, last 3 are 0)
              // which is 0xc8 in hex
              QVERIFY(masked.address()[5] == 0xc8);

              //Assert the remainder bytes are all zero
              QVERIFY(bytesFromIndexToIndexEqualToZero(6, sizeof(masked.address()), masked.address()));
          }
          // 2001:abcd::0/32
          {
              Ipv6Address address("2001:abcd::0");
              Ipv6Address masked = Ipv6Address::maskIpv6(address, 32);

              QVERIFY(masked.address()[3] == 0xcd);
          }

          // 2001:abcd::0/32 (use raw address overload)
          {
              Ipv6Address address("2001:abcd::0");
              Ipv6Address masked = Ipv6Address::maskIpv6(address.address(), 32);

              QVERIFY(masked.address()[3] == 0xcd);
          }
      }

      void testIpv6AddressinSubnet()
      {
          // 2001:abcd::0/32
          {
              Ipv6Address address1("2001:abcd:abcd::0");
              Ipv6Address address2("2001:abcd::0");
              QVERIFY(address1.inSubnet(address2, 32));
              QVERIFY(address1.inSubnet(address2.address(), 32));
           }

           // 2001:abcd::0/31
          {
              Ipv6Address address1("2001:abcd:abcd::0");
              Ipv6Address address2("2001:abcd::0");
              QVERIFY(address1.inSubnet(address2, 31));
           }

           // 2001:abcd::0/28
          {
              Ipv6Address address1("2001:abcd:abcd::0");
              Ipv6Address address2("2001:abcd::0");
              QVERIFY(address1.inSubnet(address2, 28));
           }

           // 2001:adcd::0/28
           {
              Ipv6Address address1("2001:abcd:abcd::0");
              Ipv6Address address2("2001:adcd::0");
              QVERIFY(!address1.inSubnet(address2, 28));
              QVERIFY(!address1.inSubnet(address2.address(), 28));
           }

           // 2001:abcd:cafe:babe/128
           {
               Ipv6Address address1{"2001:abcd:cafe:babe::1"};
               Ipv6Address address2{"2001:abcd:cafe:babe::0"};
               QVERIFY(!address1.inSubnet(address2, 128));
               QVERIFY(!address1.inSubnet(address2.address(), 128));
           }

           // 2001:abcd:cafe:babe/127
           {
               Ipv6Address address1{"2001:abcd:cafe:babe::1"};
               Ipv6Address address2{"2001:abcd:cafe:babe::0"};
               QVERIFY(address1.inSubnet(address2, 127));
               QVERIFY(address1.inSubnet(address2.address(), 127));
           }

           // 0/128
           {
               Ipv6Address address1{"::0"};
               Ipv6Address address2{"::0"};
               QVERIFY(address1.inSubnet(address2, 128));
               QVERIFY(address1.inSubnet(address2.address(), 128));
           }
           // 0/300
           {
               Ipv6Address address1{"::0"};
               Ipv6Address address2{"::0"};
               // Values of 128 clipped to 128
               QVERIFY(address1.inSubnet(address2, 300));
           }
           // 2001::1/0 (zero prefix length - matches everything)
           {
               Ipv6Address address1{"2001::1"};
               Ipv6Address address2{};
               Ipv6Address address3{"fabc:cafe:beef::1"};
               QVERIFY(address1.inSubnet(address2, 0));
               QVERIFY(address3.inSubnet(address2, 0));
           }

       }

      void testMaskIpv4()
      {
          // An int where all bits are set to 1
          const uint32_t allOnes{~std::uint32_t{0}};

          // /32
          {
              Ipv4Address address{"1.1.1.1"};
              Ipv4Address masked = Ipv4Address::maskIpv4(address, 32);
              // address unchanged since /32 is the length of an ipv4 address
              QVERIFY(address == masked);
          }

          // /24
          {
              Ipv4Address address{"1.1.1.1"};
              Ipv4Address masked = Ipv4Address::maskIpv4(address, 24);
              QVERIFY((address.address() & (allOnes << 8)) == masked.address());
          }

          // Same as above, but use raw address API
          {
              Ipv4Address address{"1.1.1.1"};
              Ipv4Address masked = Ipv4Address::maskIpv4(address.address(), 24);
              QVERIFY((address.address() & (allOnes << 8)) == masked.address());
          }

          // /16
          {
              Ipv4Address address{"1.1.1.1"};
              Ipv4Address masked = Ipv4Address::maskIpv4(address, 16);
              QVERIFY((address.address() & (allOnes << 16)) == masked.address());
          }

          // /8
          {
              Ipv4Address address{"1.1.1.1"};
              Ipv4Address masked = Ipv4Address::maskIpv4(address, 8);
              QVERIFY((address.address() & (allOnes << 24)) == masked.address());
          }

          // /0 - prefix of 0 results in an empty mask
          {
              Ipv4Address address{"1.1.1.1"};
              Ipv4Address masked = Ipv4Address::maskIpv4(address, 0);
              QVERIFY(masked.isNull());
          }

          // /28 - mask extends into 4 bits of the last byte
          {
              Ipv4Address address{"255.255.255.255"};
              Ipv4Address masked = Ipv4Address::maskIpv4(address, 28);
              QVERIFY((address.address() & (allOnes << 4)) == masked.address());
          }

       }

 };
}

QTEST_GUILESS_MAIN(kapps::core::tst_ipaddress)
#include TEST_MOC
