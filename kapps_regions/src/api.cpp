// Copyright (c) 2024 Private Internet Access, Inc.
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

#include <kapps_regions/regionlist.h>
#include <kapps_regions/metadata.h>
#include "regionlist.h"
#include "metadata.h"
#include <kapps_core/src/apiguard.h>
#include <kapps_core/src/logger.h>

// Define the API struct types by deriving from the internal types.  This way,
// the internals can use the clean C++ names, and we can static_cast between the
// two without violating strict aliasing, since the types are related.
struct KARServer : public kapps::regions::Server {};
struct KARRegion : public kapps::regions::Region {};
struct KARRegionList : public kapps::regions::RegionList {};
struct KARDisplayText : public kapps::regions::DisplayText {};
struct KARDynamicRole : public kapps::regions::DynamicRole {};
struct KARCountryDisplay : public kapps::regions::CountryDisplay {};
struct KARRegionDisplay : public kapps::regions::RegionDisplay {};
struct KARMetadata : public kapps::regions::Metadata {};

namespace
{
    // The preferred structure here is to define the extern "C" APIs inside the
    // kapps::regions namespace, but that doesn't quite work on MSVC.  (It works
    // on Clang.)
    //
    // Per the spec, two "C" linkage function declarations with the same
    // unqualified name in different namespaces refer to the same function, so
    // this would be correct.
    //
    // MSVC _almost_ does this, but it seems to forget to carry over the
    // __declspec(dllexport) from the prior declaration, so the functions aren't
    // exported.  Duplicating the dllexport here works even within the
    // namespace, but this is a bit simpler.
    using namespace kapps::regions;
    using kapps::core::guard;
    using kapps::core::verify;
    using kapps::core::toApi;
    using kapps::core::fromApi;

    // Static cast is fine for Service/KARService because the values are the
    // same
    KARService toApi(Service service) {return static_cast<KARService>(service);}
    Service fromApi(KARService service) {return static_cast<Service>(service);}

    // Adapt various values between internal types and API types
    KACPortArray toApi(const Ports &ports) {return {ports.data(), ports.size()};}

    const KARServer *toApi(const Server *p) {return static_cast<const KARServer*>(p);}
    const KARRegion *toApi(const Region *p) {return static_cast<const KARRegion*>(p);}
    const KARRegionList *toApi(const RegionList *p) {return static_cast<const KARRegionList *>(p);}
    const KARDisplayText *toApi(const DisplayText *p) {return static_cast<const KARDisplayText *>(p);}
    const KARDynamicRole *toApi(const DynamicRole *p) {return static_cast<const KARDynamicRole *>(p);}
    const KARCountryDisplay *toApi(const CountryDisplay *p) {return static_cast<const KARCountryDisplay *>(p);}
    const KARRegionDisplay *toApi(const RegionDisplay *p) {return static_cast<const KARRegionDisplay *>(p);}
    const KARMetadata *toApi(const Metadata *p) {return static_cast<const KARMetadata *>(p);}

    // KARDedicatedIP and KARManualRegion both contain KARStringSliceArrays;
    // to interpret these we allocate a vector<core::StringSlice> to populate
    // the core::ArraySlice<const core::StringSlice>.  Those have to be held
    // somewhere since the DedicatedIps / ManualRegions are non-owning.
    using ServiceGroupsStorage = std::vector<std::vector<kapps::core::StringSlice>>;
    std::vector<DedicatedIp> fromApi(const KARDedicatedIP *pDips, size_t dipCount,
        ServiceGroupsStorage &serviceGroupsStorage)
    {
        verify(pDips, dipCount);

        std::vector<DedicatedIp> dips;
        dips.reserve(dipCount);
        while(dipCount)
        {
            serviceGroupsStorage.push_back(fromApi(pDips->serviceGroups));
            dips.push_back({fromApi(pDips->dipRegionId), fromApi(pDips->address),
                            fromApi(pDips->commonName), fromApi(pDips->fqdn),
                            serviceGroupsStorage.back(),
                            fromApi(pDips->correspondingRegionId)});
            ++pDips;
            --dipCount;
        }

        return dips;
    }
    std::vector<ManualRegion> fromApi(const KARManualRegion *pManualRegions, size_t manualCount,
        ServiceGroupsStorage &serviceGroupsStorage)
    {
        verify(pManualRegions, manualCount);

        std::vector<ManualRegion> manual;
        manual.reserve(manualCount);
        while(manualCount)
        {
            serviceGroupsStorage.push_back(fromApi(pManualRegions->serviceGroups));
            manual.push_back({fromApi(pManualRegions->manualRegionId),
                              fromApi(pManualRegions->address),
                              fromApi(pManualRegions->commonName),
                              fromApi(pManualRegions->fqdn),
                              serviceGroupsStorage.back(),
                              fromApi(pManualRegions->correspondingRegionId),
                              pManualRegions->forceOpenVpnNcp,
                              fromApi(pManualRegions->openVpnUdpOverridePorts),
                              fromApi(pManualRegions->openVpnTcpOverridePorts)});
            ++pManualRegions;
            --manualCount;
        }

        return manual;
    }
}

extern "C"
{
    // KARServer
    void KARServerRetain(const KARServer *pServer)
    {
        guard(pServer, [&]{pServer->retain();});
    }
    void KARServerRelease(const KARServer *pServer)
    {
        guard(pServer, [&]{pServer->release();});
    }
    KACIPv4Address KARServerIpAddress(const KARServer *pServer)
    {
        return guard(pServer, [&]{return toApi(pServer->address());});
    }
    KACStringSlice KARServerCommonName(const KARServer *pServer)
    {
        return guard(pServer, [&]{return toApi(pServer->commonName());});
    }
    KACStringSlice KARServerFqdn(const KARServer *pServer)
    {
        return guard(pServer, [&]{return toApi(pServer->fqdn());});
    }
    bool KARServerHasService(const KARServer *pServer, KARService service)
    {
        return guard(pServer, [&]{return pServer->hasService(fromApi(service));});
    }
    KACPortArray KARServerServicePorts(const KARServer *pServer, KARService service)
    {
        return guard(pServer, [&]{return toApi(pServer->servicePorts(fromApi(service)));});
    }
    bool KARServerHasOpenVpnUdp(const KARServer *pServer)
    {
        return guard(pServer, [&]{return pServer->hasOpenVpnUdp();});
    }
    KACPortArray KARServerOpenVpnUdpPorts(const KARServer *pServer)
    {
        return guard(pServer, [&]{return toApi(pServer->openVpnUdpPorts());});
    }
    bool KARServerOpenVpnUdpNcp(const KARServer *pServer)
    {
        return guard(pServer, [&]{return pServer->openVpnUdpNcp();});
    }
    bool KARServerHasOpenVpnTcp(const KARServer *pServer)
    {
        return guard(pServer, [&]{return pServer->hasOpenVpnTcp();});
    }
    KACPortArray KARServerOpenVpnTcpPorts(const KARServer *pServer)
    {
        return guard(pServer, [&]{return toApi(pServer->openVpnTcpPorts());});
    }
    bool KARServerOpenVpnTcpNcp(const KARServer *pServer)
    {
        return guard(pServer, [&]{return pServer->openVpnTcpNcp();});
    }
    bool KARServerHasWireGuard(const KARServer *pServer)
    {
        return guard(pServer, [&]{return pServer->hasWireGuard();});
    }
    KACPortArray KARServerWireGuardPorts(const KARServer *pServer)
    {
        return guard(pServer, [&]{return toApi(pServer->wireGuardPorts());});
    }
    bool KARServerHasIkev2(const KARServer *pServer)
    {
        return guard(pServer, [&]{return pServer->hasIkev2();});
    }
    bool KARServerHasShadowsocks(const KARServer *pServer)
    {
        return guard(pServer, [&]{return pServer->hasShadowsocks();});
    }
    KACPortArray KARServerShadowsocksPorts(const KARServer *pServer)
    {
        return guard(pServer, [&]{return toApi(pServer->shadowsocksPorts());});
    }
    KACStringSlice KARServerShadowsocksKey(const KARServer *pServer)
    {
        return guard(pServer, [&]{return toApi(pServer->shadowsocksKey());});
    }
    KACStringSlice KARServerShadowsocksCipher(const KARServer *pServer)
    {
        return guard(pServer, [&]{return toApi(pServer->shadowsocksCipher());});
    }
    bool KARServerHasMeta(const KARServer *pServer)
    {
        return guard(pServer, [&]{return pServer->hasMeta();});
    }
    KACPortArray KARServerMetaPorts(const KARServer *pServer)
    {
        return guard(pServer, [&]{return toApi(pServer->metaPorts());});
    }

    // KARRegion
    void KARRegionRetain(const KARRegion *pRegion)
    {
        guard(pRegion, [&]{pRegion->retain();});
    }
    void KARRegionRelease(const KARRegion *pRegion)
    {
        guard(pRegion, [&]{pRegion->release();});
    }
    KACStringSlice KARRegionId(const KARRegion *pRegion)
    {
        return guard(pRegion, [&]{return toApi(pRegion->id());});
    }
    bool KARRegionAutoSafe(const KARRegion *pRegion)
    {
        return guard(pRegion, [&]{return pRegion->autoSafe();});
    }
    bool KARRegionPortForward(const KARRegion *pRegion)
    {
        return guard(pRegion, [&]{return pRegion->portForward();});
    }
    bool KARRegionGeoLocated(const KARRegion *pRegion)
    {
        return guard(pRegion, [&]{return pRegion->geoLocated();});
    }
    bool KARRegionOffline(const KARRegion *pRegion)
    {
        return guard(pRegion, [&]{return pRegion->offline();});
    }
    bool KARRegionIsDedicatedIp(const KARRegion *pRegion)
    {
        return guard(pRegion, [&]{return pRegion->isDedicatedIp();});
    }
    KACIPv4Address KARRegionDedicatedIpAddress(const KARRegion *pRegion)
    {
        return guard(pRegion, [&]{return toApi(pRegion->dipAddress());});
    }
    bool KARRegionHasService(const KARRegion *pRegion, KARService service)
    {
        return guard(pRegion, [&]{return pRegion->hasService(fromApi(service));});
    }
    const KARServer *KARRegionFirstServerFor(const KARRegion *pRegion, KARService service)
    {
        return guard(pRegion, [&]{return toApi(pRegion->firstServerFor(fromApi(service)));});
    }
    KACArraySlice KARRegionServers(const KARRegion *pRegion)
    {
        return guard(pRegion, [&]{return toApi(pRegion->servers());});
    }

    // KARRegionList
    const KARRegionList *KARRegionListCreate(KACStringSlice regionsJson,
                                             KACStringSlice shadowsocksJson,
                                             const KARDedicatedIP *pDIPs,
                                             size_t dipCount,
                                             const KARManualRegion *pManualRegions,
                                             size_t manualCount)
    {
        return guard([&]
        {
            ServiceGroupsStorage serviceGroupsStorage;
            auto dips = fromApi(pDIPs, dipCount, serviceGroupsStorage);
            auto manual = fromApi(pManualRegions, manualCount, serviceGroupsStorage);
            return toApi(new RegionList{fromApi(regionsJson),
                fromApi(shadowsocksJson), dips, manual});
        });
    }
    const KARRegionList *KARRegionListCreatePiav6(KACStringSlice regionsv6Json,
                                                  KACStringSlice shadowsocksJson,
                                                  const KARDedicatedIP *pDIPs,
                                                  size_t dipCount,
                                                  const KARManualRegion *pManualRegions,
                                                  size_t manualCount)
    {
        return guard([&]
        {
            ServiceGroupsStorage serviceGroupsStorage;
            auto dips = fromApi(pDIPs, dipCount, serviceGroupsStorage);
            auto manual = fromApi(pManualRegions, manualCount, serviceGroupsStorage);
            return toApi(new RegionList{RegionList::PIAv6, fromApi(regionsv6Json),
                fromApi(shadowsocksJson), dips, manual});
        });
    }
    void KARRegionListDestroy(const KARRegionList *pRegionList)
    {
        guard(pRegionList, [&]
        {
            const kapps::regions::RegionList *pImpl{pRegionList};
            delete pImpl;
        });
    }
    KACArraySlice KARRegionListPublicDnsServers(const KARRegionList *pRegionList)
    {
        return guard(pRegionList, [&]{return toApi(pRegionList->publicDnsServers());});
    }
    const KARRegion *KARRegionListGetRegion(const KARRegionList *pRegionList,
                                            KACStringSlice id)
    {
        return guard(pRegionList, [&]{return toApi(pRegionList->getRegion(fromApi(id)));});
    }
    KACArraySlice KARRegionListRegions(const KARRegionList *pRegionList)
    {
        return guard(pRegionList, [&]{return toApi(pRegionList->regions());});
    }

    // KARDisplayText
    KACStringSlice KARDisplayTextGetLanguageText(const KARDisplayText *pDisplayText,
                                                 KACStringSlice language)
    {
        return guard(pDisplayText, [&]
        {
            Bcp47Tag tag{fromApi(language)};
            return toApi(pDisplayText->getLanguageText(tag));
        });
    }

    // KARDynamicRole
    void KARDynamicRoleRetain(const KARDynamicRole *pDynGroup)
    {
        guard(pDynGroup, [&]{pDynGroup->retain();});
    }
    void KARDynamicRoleRelease(const KARDynamicRole *pDynGroup)
    {
        guard(pDynGroup, [&]{pDynGroup->release();});
    }
    KACStringSlice KARDynamicRoleId(const KARDynamicRole *pDynGroup)
    {
        return guard(pDynGroup, [&]{return toApi(pDynGroup->id());});
    }
    const KARDisplayText *KARDynamicRoleName(const KARDynamicRole *pDynGroup)
    {
        return guard(pDynGroup, [&]{return toApi(&pDynGroup->name());});
    }
    KACStringSlice KARDynamicRoleResource(const KARDynamicRole *pDynGroup)
    {
        return guard(pDynGroup, [&]{return toApi(pDynGroup->resource());});
    }
    KACStringSlice KARDynamicRoleWinIcon(const KARDynamicRole *pDynGroup)
    {
        return guard(pDynGroup, [&]{return toApi(pDynGroup->winIcon());});
    }

    // KARCountryDisplay
    void KARCountryDisplayRetain(const KARCountryDisplay *pCountry)
    {
        guard(pCountry, [&]{pCountry->retain();});
    }
    void KARCountryDisplayRelease(const KARCountryDisplay *pCountry)
    {
        guard(pCountry, [&]{pCountry->release();});
    }
    KACStringSlice KARCountryDisplayCode(const KARCountryDisplay *pCountry)
    {
        return guard(pCountry, [&]{return toApi(pCountry->code());});
    }
    const KARDisplayText *KARCountryDisplayName(const KARCountryDisplay *pCountry)
    {
        return guard(pCountry, [&]{return toApi(&pCountry->name());});
    }
    const KARDisplayText *KARCountryDisplayPrefix(const KARCountryDisplay *pCountry)
    {
        return guard(pCountry, [&]{return toApi(&pCountry->prefix());});
    }

    // KARRegionDisplay
    void KARRegionDisplayRetain(const KARRegionDisplay *pRegion)
    {
        guard(pRegion, [&]{pRegion->retain();});
    }
    void KARRegionDisplayRelease(const KARRegionDisplay *pRegion)
    {
        guard(pRegion, [&]{pRegion->release();});
    }
    KACStringSlice KARRegionDisplayId(const KARRegionDisplay *pRegion)
    {
        return guard(pRegion, [&]{return toApi(pRegion->id());});
    }
    KACStringSlice KARRegionDisplayCountry(const KARRegionDisplay *pRegion)
    {
        return guard(pRegion, [&]{return toApi(pRegion->country());});
    }
    double KARRegionDisplayGeoLatitude(const KARRegionDisplay *pRegion)
    {
        return guard(pRegion, [&]{return pRegion->geoLatitude();});
    }
    double KARRegionDisplayGeoLongitude(const KARRegionDisplay *pRegion)
    {
        return guard(pRegion, [&]{return pRegion->geoLongitude();});
    }
    const KARDisplayText *KARRegionDisplayName(const KARRegionDisplay *pRegion)
    {
        return guard(pRegion, [&]{return toApi(&pRegion->name());});
    }

    // KARMetadata
    const KARMetadata *KARMetadataCreate(KACStringSlice metadataJson,
                                         const KARDedicatedIP *pDIPs,
                                         size_t dipCount,
                                         const KARManualRegion *pManualRegions,
                                         size_t manualCount)
    {
        return guard([&]
        {
            ServiceGroupsStorage serviceGroupsStorage;
            auto dips = fromApi(pDIPs, dipCount, serviceGroupsStorage);
            auto manual = fromApi(pManualRegions, manualCount, serviceGroupsStorage);
            return toApi(new Metadata{fromApi(metadataJson), dips, manual});
        });
    }
    const KARMetadata *KARMetadataCreatePiav6v2(KACStringSlice regionsv6Json,
                                                KACStringSlice metadatav2Json,
                                                const KARDedicatedIP *pDIPs,
                                                size_t dipCount,
                                                const KARManualRegion *pManualRegions,
                                                size_t manualCount)
    {
        return guard([&]
        {
            ServiceGroupsStorage serviceGroupsStorage;
            auto dips = fromApi(pDIPs, dipCount, serviceGroupsStorage);
            auto manual = fromApi(pManualRegions, manualCount, serviceGroupsStorage);
            return toApi(new Metadata{fromApi(regionsv6Json), fromApi(metadatav2Json), dips, manual});
        });
    }
    void KARMetadataDestroy(const KARMetadata *pMetadata)
    {
        guard(pMetadata, [&]
        {
            const kapps::regions::Metadata *pImpl{pMetadata};
            delete pImpl;
        });
    }
    const KARDynamicRole *KARMetadataGetDynamicRole(const KARMetadata *pMetadata,
                                                      KACStringSlice id)
    {
        return guard(pMetadata, [&]{return toApi(pMetadata->getDynamicRole(fromApi(id)));});
    }
    KACArraySlice KARMetadataDynamicRoles(const KARMetadata *pMetadata)
    {
        return guard(pMetadata, [&]{return toApi(pMetadata->dynamicGroups());});
    }
    const KARCountryDisplay *KARMetadataGetCountryDisplay(const KARMetadata *pMetadata,
                                                      KACStringSlice code)
    {
        return guard(pMetadata, [&]{return toApi(pMetadata->getCountryDisplay(fromApi(code)));});
    }
    KACArraySlice KARMetadataCountryDisplays(const KARMetadata *pMetadata)
    {
        return guard(pMetadata, [&]{return toApi(pMetadata->countryDisplays());});
    }
    const KARRegionDisplay *KARMetadataGetRegionDisplay(const KARMetadata *pMetadata,
                                                      KACStringSlice id)
    {
        return guard(pMetadata, [&]{return toApi(pMetadata->getRegionDisplay(fromApi(id)));});
    }
    KACArraySlice KARMetadataRegionDisplays(const KARMetadata *pMetadata)
    {
        return guard(pMetadata, [&]{return toApi(pMetadata->regionDisplays());});
    }
}
