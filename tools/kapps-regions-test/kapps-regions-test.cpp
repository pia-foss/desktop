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

#include <kapps_core/logger.h>
#include <kapps_regions/regionlist.h>
#include <kapps_regions/metadata.h>
#include <iostream>
#include <cstring>
#include <cassert>
#include <vector>
#include <cstdint>

std::ostream &operator<<(std::ostream &os, const KACStringSlice &str)
{
    return os.write(str.data, str.size);
}

const char *getLevelName(int level)
{
    switch(level)
    {
        case KAPPS_CORE_LOG_MESSAGE_LEVEL_FATAL:
            return "fatal";
        case KAPPS_CORE_LOG_MESSAGE_LEVEL_ERROR:
            return "error";
        case KAPPS_CORE_LOG_MESSAGE_LEVEL_WARNING:
            return "warning";
        case KAPPS_CORE_LOG_MESSAGE_LEVEL_INFO:
            return "info";
        case KAPPS_CORE_LOG_MESSAGE_LEVEL_DEBUG:
            return "debug";
        default:
            return "??";
    }
}

// Logging sink for the test - logs to stdout
void writeLogMsg(void *pContext, const ::KACLogMessage *pMessage)
{
    assert(pMessage);   // Guaranteed by the logger

    // The actual message can contain line breaks (this often happens when
    // tracing tool output, config dumps, etc.), so a real implementation may
    // want to split the message on line breaks and emit the line prefix for
    // each line.
    std::cout << "[" << pMessage->module << "][" << pMessage->category
        << "][" << getLevelName(pMessage->level) << "][" << pMessage->file
        << "][" << pMessage->line << "] " << pMessage->pMessage << std::endl;
}

void initLogging()
{
    // Enable logging and set up a logging sink (using the C-linkage public API)
    ::KACEnableLogging(true);
    // Use our log sink as the callback.  We don't need any context in this
    // example.
    ::KACLogCallback sinkCallback{};
    sinkCallback.pWriteFn = &writeLogMsg;
    // KACLogInit copies the callback struct, so we can safely let
    // sinkCallback be destroyed
    ::KACLogInit(&sinkCallback);
}

std::ostream &operator<<(std::ostream &os, const KACPortArray &ports)
{
    const uint16_t *pPort = ports.data;
    size_t portCount = ports.size;
    os << "{";

    // No comma before first port
    if(portCount)
    {
        os << *pPort;
        ++pPort;
        --portCount;
    }

    while(portCount)
    {
        os << ", " << *pPort;
        ++pPort;
        --portCount;
    }

    os << "}";
    return os;
}

KACStringSlice getLangText(const KARDisplayText *pDt, const char *lang)
{
    return KARDisplayTextGetLanguageText(pDt, {lang, std::strlen(lang)});
}

// "Print" a KARDisplayText by testing a few specific languages
std::ostream &operator<<(std::ostream &os, const KARDisplayText *pDt)
{
    os << "{";
    os << "en-US: " << getLangText(pDt, "en-US") << ", ";
    os << "es-MX: " << getLangText(pDt, "es-MX");
    os << "}";
    return os;
}

void advance(KACArraySlice &slice)
{
    // Manually advance the data pointer by 'stride' bytes
    slice.data = reinterpret_cast<const void*>(reinterpret_cast<std::size_t>(slice.data) + slice.stride);
    --slice.size;
}

KACStringSlice apiSlice(const char *str)
{
    return {str, std::strlen(str)};
}

KACIPv4Address apiIpv4(std::uint8_t a, std::uint8_t b, std::uint8_t c, std::uint8_t d)
{
    KACIPv4Address addr{};
    addr = a;
    addr <<= 8;
    addr |= b;
    addr <<= 8;
    addr |= c;
    addr <<= 8;
    addr |= d;
    return addr;
}

const class DipManualRegions
{
public:
    DipManualRegions()
    {
        _manual.push_back({apiSlice("manual"), apiIpv4(138, 199, 10, 64),
            apiSlice("newyork420"), apiSlice(""),
            {_manualServiceGroups.data(), _manualServiceGroups.size()},
            apiSlice("us_chicago"), false, KACPortArray{}, KACPortArray{}});

        _dips.push_back({apiSlice("dip-abcd01234"), apiIpv4(2, 3, 4, 5),
            apiSlice("chicago400"), apiSlice(""),
            {_dipServiceGroups.data(), _dipServiceGroups.size()},
            apiSlice("us_chicago")});
    }

public:
    const std::vector<KARManualRegion> &manual() const {return _manual;}
    const std::vector<KARDedicatedIP> &dips() const {return _dips;}

private:
    std::vector<KARManualRegion> _manual;
    std::vector<KACStringSlice> _manualServiceGroups{apiSlice("traffic1")};
    std::vector<KARDedicatedIP> _dips;
    std::vector<KACStringSlice> _dipServiceGroups{apiSlice("traffic1")};
} dipManualRegions;

void testRegionList()
{
    const char *regionsJson = R"(
        {
          "service_configs": [
            {
              "name": "traffic1",
              "services": [
                {"service":"openvpn_udp", "ports":[8080,853,123,53], "ncp":false},
                {"service":"openvpn_tcp", "ports":[80,443,853,8443], "ncp":false},
                {"service":"wireguard", "ports":[1337]},
                {"service":"ikev2"}
              ]
            },
            {
              "name": "meta",
              "services": [
                {"service":"meta", "ports":[443,8080]}
              ]
            }
          ],
          "pubdns": ["1.2.3.4", "5.6.7.8"],
          "regions": [
            {
              "id": "us_chicago",
              "auto_region": true,
              "port_forward": false,
              "geo": false,
              "servers": [
                {"ip":"154.21.23.79", "cn":"chicago412",
                    "fqdn":"blade9.chicago-rack412.nodes.gen4.ninja",
                    "service_config":"traffic1"},
                {"ip":"154.21.114.233", "cn":"chicago421",
                    "fqdn":"blade7.chicago-rack421.nodes.gen4.ninja",
                    "service_config":"traffic1"},
                {"ip":"212.102.59.129", "cn":"chicago403", "fqdn":"",
                    "service_config":"meta"},
                {"ip":"154.21.28.4", "cn":"chicago405", "service_config":"meta"}
              ]
            },
            {
              "id": "spain",
              "auto_region": true,
              "port_forward": true,
              "geo": false,
              "servers": [
                {"ip":"212.102.49.78", "cn":"madrid401", "service_config":"traffic1"},
                {"ip":"212.102.49.6", "cn":"madrid402", "service_config":"traffic1"},
                {"ip":"195.181.167.33", "cn":"madrid401", "service_config":"meta"},
                {"ip":"212.102.49.1", "cn":"madrid402", "service_config":"meta"}
              ]
            },
            {
              "id": "aus_perth",
              "auto_region": true,
              "port_forward": true,
              "geo": false,
              "servers": [
                {"ip":"179.61.228.124", "cn":"perth404", "service_config":"traffic1"},
                {"ip":"179.61.228.170", "cn":"perth405", "service_config":"traffic1"},
                {"ip":"43.250.205.57", "cn":"perth403", "service_config":"meta"},
                {"ip":"43.250.205.178", "cn":"perth404", "service_config":"meta"}
              ]
            }
          ]
        }
    )";


    const KARRegionList *pRgnList = KARRegionListCreate(
        apiSlice(regionsJson), {},
        dipManualRegions.dips().data(), dipManualRegions.dips().size(),
        dipManualRegions.manual().data(), dipManualRegions.manual().size());

    auto pubdns = KARRegionListPublicDnsServers(pRgnList);
    std::cout << pubdns.size << " public DNS servers" << std::endl;
    while(pubdns.size)
    {
        auto addr = reinterpret_cast<const KACIPv4Address*>(pubdns.data);
        std::cout << "  " << *addr << std::endl;
        advance(pubdns);
    }

    auto regions = KARRegionListRegions(pRgnList);

    std::cout << regions.size << " regions" << std::endl;
    while(regions.size)
    {
        auto pRgn = *reinterpret_cast<const KARRegion * const *>(regions.data);
        std::cout << KARRegionId(pRgn) << std::endl;
        std::cout << "  auto safe: " << KARRegionAutoSafe(pRgn) << std::endl;
        std::cout << "  port fwd:  " << KARRegionPortForward(pRgn) << std::endl;
        std::cout << "  geo loc.:  " << KARRegionGeoLocated(pRgn) << std::endl;
        std::cout << "  offline:   " << KARRegionOffline(pRgn) << std::endl;
        std::cout << "  ded. ip:   " << KARRegionIsDedicatedIp(pRgn) << std::endl;
        std::cout << "  dip addr:  " << KARRegionDedicatedIpAddress(pRgn) << std::endl;
        std::cout << "  has ovudp: " << KARRegionHasService(pRgn, KARServiceOpenVpnUdp) << std::endl;
        std::cout << "  has ovtcp: " << KARRegionHasService(pRgn, KARServiceOpenVpnTcp) << std::endl;
        std::cout << "  has ikev2: " << KARRegionHasService(pRgn, KARServiceIkev2) << std::endl;
        std::cout << "  has wg:    " << KARRegionHasService(pRgn, KARServiceWireGuard) << std::endl;
        std::cout << "  has ss:    " << KARRegionHasService(pRgn, KARServiceShadowsocks) << std::endl;
        std::cout << "  has meta:  " << KARRegionHasService(pRgn, KARServiceMeta) << std::endl;

        // Also try to get the region by ID to test that API
        auto pRgn2 = KARRegionListGetRegion(pRgnList, KARRegionId(pRgn));
        // Should be the same, and pRgn2 should equal pRgn
        std::cout << "  re-check:  " << KARRegionId(pRgn2) << " - " << (pRgn2 == pRgn) << std::endl;

        auto servers = KARRegionServers(pRgn);
        std::cout << "  servers (" << servers.size << ")" << std::endl;
        while(servers.size)
        {
            auto pServer = *reinterpret_cast<const KARServer * const *>(servers.data);
            std::cout << "    " << KARServerIpAddress(pServer) << " - "
                << KARServerCommonName(pServer) << std::endl;
            std::cout << "      fqdn:      " << KARServerFqdn(pServer) << std::endl;
            std::cout << "      has ovudp: " << KARServerHasService(pServer, KARServiceOpenVpnUdp) << std::endl;
            std::cout << "      has ovtcp: " << KARServerHasService(pServer, KARServiceOpenVpnTcp) << std::endl;
            std::cout << "      has ikev2: " << KARServerHasService(pServer, KARServiceIkev2) << std::endl;
            std::cout << "      has wg:    " << KARServerHasService(pServer, KARServiceWireGuard) << std::endl;
            std::cout << "      has ss:    " << KARServerHasService(pServer, KARServiceShadowsocks) << std::endl;
            std::cout << "      has meta:  " << KARServerHasService(pServer, KARServiceMeta) << std::endl;
            std::cout << "      ovpn udp:  " << KARServerOpenVpnUdpPorts(pServer) << std::endl;
            std::cout << "      ovpn tcp:  " << KARServerOpenVpnTcpPorts(pServer) << std::endl;
            std::cout << "      wireguard: " << KARServerWireGuardPorts(pServer) << std::endl;
            std::cout << "      shadows.:  " << KARServerShadowsocksPorts(pServer) << std::endl;
            std::cout << "      meta:      " << KARServerMetaPorts(pServer) << std::endl;

            advance(servers);
        }
        advance(regions);
    }

    KARRegionListDestroy(pRgnList);
}

void testMetadata()
{
    const char *metadataJson = R"(
        {
          "dynamic_roles": [
            {
              "id":"8",
              "display":{
                "de": "NoSpy-Server",
                "en-US": "NoSpy Servers",
                "es-MX": "Servidores NoSpy"
              },
              "resource":"nospy",
              "icons":{"win":"https://download.example.com/nospy.svg"}
            },
            {
              "id":"19",
              "display":{
                "de": "Für Gaming",
                "en-US": "For Gaming",
                "es-MX": "Para juegos"
              },
              "resource":"gaming",
              "icons":{"win":"https://download.example.com/gaming.svg"}
            }
          ],
          "countries": [
            {
              "code":"US",
              "display": {
                "ar": "الولايات المتحدة",
                "da": "USA",
                "de": "USA",
                "en-US": "United States",
                "es-MX": "Estados Unidos",
                "fr": "États-Unis",
                "it": "Stati Uniti",
                "ja": "アメリカ",
                "ko": "미국",
                "nb": "USA",
                "nl": "Verenigde Staten",
                "pl": "Stany Zjednoczone",
                "pt-BR": "Estados Unidos",
                "ru": "Соединенные Штаты",
                "sv": "USA",
                "th": "สหรัฐอเมริกา",
                "tr": "Birleşik Devletler",
                "zh-Hans": "美国",
                "zh-Hant": "美國"
              },
              "prefix": {
                "ar": "الولايات ",
                "da": "USA ",
                "de": "USA ",
                "en-US": "US ",
                "es-MX": "EE. UU. ",
                "fr": "US ",
                "it": "US ",
                "ja": "米国 ",
                "ko": "미국 ",
                "nb": "US ",
                "nl": "VS ",
                "pl": "USA ",
                "pt-BR": "US ",
                "ru": "США - ",
                "sv": "US ",
                "th": "สหรัฐอเมริกา ",
                "tr": "ABD ",
                "zh-Hans": "美国",
                "zh-Hant": "美國"
              }
            },
            {
              "code":"ES",
              "display": {},
              "prefix": {}
            },
            {
              "code":"AU",
              "display": {},
              "prefix": {}
            }
          ],
          "regions": [
            {
              "id": "us_chicago",
              "country": "US",
              "geo": [41.883229, -87.632398],
              "display": {}
            },
            {
              "id": "spain",
              "country": "ES",
              "geo": [40.416705, -3.703583],
              "display": {}
            },
            {
              "id": "aus_perth",
              "country": "AU",
              "geo": [-31.952712, 115.86048],
              "display": {}
            }
          ]
        }
    )";
    const KARMetadata *pMetadata = KARMetadataCreate(
        {metadataJson, std::strlen(metadataJson)},
        dipManualRegions.dips().data(), dipManualRegions.dips().size(),
        dipManualRegions.manual().data(), dipManualRegions.manual().size());

    auto dynGroups = KARMetadataDynamicRoles(pMetadata);
    std::cout << dynGroups.size << " dyn. groups" << std::endl;
    while(dynGroups.size)
    {
        auto pDG = *reinterpret_cast<const KARDynamicRole * const *>(dynGroups.data);
        std::cout << KARDynamicRoleId(pDG) << std::endl;
        std::cout << "  name:      " << KARDynamicRoleName(pDG) << std::endl;
        std::cout << "  resource:  " << KARDynamicRoleResource(pDG) << std::endl;
        std::cout << "  win icon:  " << KARDynamicRoleWinIcon(pDG) << std::endl;

        // Test lookup by ID
        auto pDG2 = KARMetadataGetDynamicRole(pMetadata, KARDynamicRoleId(pDG));
        std::cout << "  re-check:  " << KARDynamicRoleId(pDG2) << " - " << (pDG2 == pDG) << std::endl;

        advance(dynGroups);
    }

    auto countries = KARMetadataCountryDisplays(pMetadata);
    std::cout << countries.size << " country displays" << std::endl;
    while(countries.size)
    {
        auto pCountry = *reinterpret_cast<const KARCountryDisplay * const *>(countries.data);
        std::cout << KARCountryDisplayCode(pCountry) << std::endl;
        std::cout << "  name:      " << KARCountryDisplayName(pCountry) << std::endl;
        std::cout << "  prefix:    " << KARCountryDisplayPrefix(pCountry) << std::endl;

        // Test lookup by ID
        auto pCountry2 = KARMetadataGetCountryDisplay(pMetadata, KARCountryDisplayCode(pCountry));
        std::cout << "  re-check:  " << KARCountryDisplayCode(pCountry2) << " - " << (pCountry2 == pCountry) << std::endl;
        advance(countries);
    }

    auto regions = KARMetadataRegionDisplays(pMetadata);
    std::cout << regions.size << " region displays" << std::endl;
    while(regions.size)
    {
        auto pRegion = *reinterpret_cast<const KARRegionDisplay * const *>(regions.data);
        std::cout << KARRegionDisplayId(pRegion) << std::endl;
        std::cout << "  country:   " << KARRegionDisplayCountry(pRegion) << std::endl;
        std::cout << "  geo. lat.: " << KARRegionDisplayGeoLatitude(pRegion) << std::endl;
        std::cout << "  geo. lon.: " << KARRegionDisplayGeoLongitude(pRegion) << std::endl;
        std::cout << "  name:      " << KARRegionDisplayName(pRegion) << std::endl;

        // Test lookup by ID
        auto pRegion2 = KARMetadataGetRegionDisplay(pMetadata, KARRegionDisplayId(pRegion));
        std::cout << "  re-check:  " << KARRegionDisplayId(pRegion2) << " - " << (pRegion2 == pRegion) << std::endl;

        advance(regions);
    }

    // Test fallback cases in KARDisplayText
    const KARCountryDisplay *pUS = KARMetadataGetCountryDisplay(pMetadata, apiSlice("US"));
    const KARDisplayText *pDT = pUS ? KARCountryDisplayName(pUS) : nullptr;
    if(!pDT)
    {
        std::cout << "failed to find display text to test fallback" << std::endl;
    }
    else
    {
        std::cout << "display text fallback:" << std::endl;
        // There are no languages in use that require a complete lang-Script-RGN
        // lang-Script:
        std::cout << "  zh-Hans:               " << getLangText(pDT, "zh-Hans") << std::endl;
        std::cout << "  zh-Hant:               " << getLangText(pDT, "zh-Hant") << std::endl;
        // lang-RGN:
        std::cout << "  pt-BR:                 " << getLangText(pDT, "pt-BR") << std::endl;
        std::cout << "  en-US:                 " << getLangText(pDT, "en-US") << std::endl;
        // lang:
        std::cout << "  fr:                    " << getLangText(pDT, "fr") << std::endl;
        // Fallback from lang-Script-RGN to lang-Script
        std::cout << "  zh-Hans-CN -> zh-Hans: " << getLangText(pDT, "zh-Hans-CN") << std::endl;
        // Fallback from lang-Script-RGN to lang-RGN
        std::cout << "  pt-Latn-BR -> pt-BR:   " << getLangText(pDT, "pt-Latn-BR") << std::endl;
        // Fallback to just <lang>
        std::cout << "  it-IT -> it:           " << getLangText(pDT, "it-IT") << std::endl;
        // Fallback to en-US
        std::cout << "  zz -> en-US:           " << getLangText(pDT, "zz") << std::endl;
        // Error
        std::cout << "  bogus -> <error>:      " << getLangText(pDT, "bogus") << std::endl;
    }

    KARMetadataDestroy(pMetadata);
}

int main(void)
{
    initLogging();
    testRegionList();
    testMetadata();
    return 0;
}
