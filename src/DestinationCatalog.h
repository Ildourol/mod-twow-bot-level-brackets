#ifndef BRACKETS_DESTINATION_CATALOG_H
#define BRACKETS_DESTINATION_CATALOG_H

#include "BracketsCommon.h"
#include <string>
#include <vector>
#include <unordered_set>
#include <cstdint>

namespace Brackets
{
    struct LandingPoint
    {
        uint32 mapId{0};
        float  x{0.0f};
        float  y{0.0f};
        float  z{0.0f};
        float  o{0.0f};
    };

    struct CityInfo
    {
        CityId       id;
        const char*  name;
        uint8        teamId; // 0 = Alliance, 1 = Horde
        uint32       mapId;
        std::unordered_set<uint32> areaIds;
        LandingPoint landing;
        float        cx, cy, cz, radius; // Geometric fallback sphere
    };

    struct OutdoorRegion
    {
        uint32      regionId;
        const char* name;
        uint32      mapId;
        uint32      zoneId;
        std::unordered_set<uint32> areaIds;
        uint8       teamAccess; // 0 = Alliance only, 1 = Horde only, 2 = Contested/Both
        uint8       minLevel;
        uint8       maxLevel;
        float       weight;
    };

    class DestinationCatalog
    {
    public:
        static DestinationCatalog& Instance()
        {
            static DestinationCatalog instance;
            return instance;
        }

        void Initialize();

        const CityInfo* GetCity(CityId id) const;
        const CityInfo* FindCityByName(const std::string& name) const;
        const CityInfo* GetCityForArea(uint32 mapId, uint32 areaId, float x, float y, float z) const;
        bool IsInCity(CityId id, uint32 mapId, uint32 areaId, float x, float y, float z) const;
        bool IsInAnyCity(uint32 mapId, uint32 areaId, float x, float y, float z) const;

        const std::vector<OutdoorRegion>& GetOutdoorRegions() const { return m_regions; }
        const OutdoorRegion* GetRegion(uint32 regionId) const;
        const OutdoorRegion* FindRegionByName(const std::string& name) const;
        const OutdoorRegion* GetRegionForLocation(uint32 mapId, uint32 areaId, uint32 zoneId) const;

        std::vector<const OutdoorRegion*> GetRegionsForLevelAndFaction(uint8 level, uint8 teamId) const;

    private:
        DestinationCatalog();
        ~DestinationCatalog() = default;

        std::vector<CityInfo> m_cities;
        std::vector<OutdoorRegion> m_regions;
    };
}

#endif // BRACKETS_DESTINATION_CATALOG_H
