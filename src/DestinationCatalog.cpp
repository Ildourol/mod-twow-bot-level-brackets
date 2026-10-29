#include "DestinationCatalog.h"
#include <cmath>
#include <algorithm>

namespace Brackets
{
    DestinationCatalog::DestinationCatalog()
    {
        Initialize();
    }

    void DestinationCatalog::Initialize()
    {
        m_cities.clear();
        m_cities.reserve(CITY_COUNT);

        // 0. Stormwind City (Alliance, Map 0, Area 1519)
        {
            CityInfo sw;
            sw.id = CITY_STORMWIND;
            sw.name = "Stormwind";
            sw.teamId = 0;
            sw.mapId = 0;
            sw.areaIds = { 1519, 1497, 12, 1437 }; // Core Stormwind subareas
            sw.landing = { 0, -8833.38f, 628.62f, 94.09f, 1.0f };
            sw.cx = -8913.23f; sw.cy = 554.63f; sw.cz = 93.79f; sw.radius = 700.0f;
            m_cities.push_back(sw);
        }

        // 1. Ironforge (Alliance, Map 0, Area 1537)
        {
            CityInfo ifg;
            ifg.id = CITY_IRONFORGE;
            ifg.name = "Ironforge";
            ifg.teamId = 0;
            ifg.mapId = 0;
            ifg.areaIds = { 1537 };
            ifg.landing = { 0, -4918.88f, -940.40f, 501.55f, 5.42f };
            ifg.cx = -4918.88f; ifg.cy = -940.40f; ifg.cz = 501.55f; ifg.radius = 500.0f;
            m_cities.push_back(ifg);
        }

        // 2. Darnassus (Alliance, Map 1, Area 1657)
        {
            CityInfo dar;
            dar.id = CITY_DARNASSUS;
            dar.name = "Darnassus";
            dar.teamId = 0;
            dar.mapId = 1;
            dar.areaIds = { 1657 };
            dar.landing = { 1, 9947.52f, 2482.73f, 1316.21f, 2.23f };
            dar.cx = 9947.52f; dar.cy = 2482.73f; dar.cz = 1316.21f; dar.radius = 650.0f;
            m_cities.push_back(dar);
        }

        // 3. Alah'Thalas (Alliance, Turtle custom High Elf capital, Map 0, Area 2040/5225)
        {
            CityInfo at;
            at.id = CITY_ALAH_THALAS;
            at.name = "Alah'Thalas";
            at.teamId = 0;
            at.mapId = 0;
            at.areaIds = { 2040, 5225 };
            at.landing = { 0, 3180.25f, -3450.15f, 138.80f, 4.70f };
            at.cx = 3180.25f; at.cy = -3450.15f; at.cz = 138.80f; at.radius = 450.0f;
            m_cities.push_back(at);
        }

        // 4. Orgrimmar (Horde, Map 1, Area 1637)
        {
            CityInfo org;
            org.id = CITY_ORGRIMMAR;
            org.name = "Orgrimmar";
            org.teamId = 1;
            org.mapId = 1;
            org.areaIds = { 1637, 2177, 2178 };
            org.landing = { 1, 1633.00f, -4439.11f, 15.76f, 0.17f };
            org.cx = 1633.00f; org.cy = -4439.11f; org.cz = 15.76f; org.radius = 650.0f;
            m_cities.push_back(org);
        }

        // 5. Undercity (Horde, Map 0, Area 1497)
        {
            CityInfo uc;
            uc.id = CITY_UNDERCITY;
            uc.name = "Undercity";
            uc.teamId = 1;
            uc.mapId = 0;
            uc.areaIds = { 1497 };
            uc.landing = { 0, 1633.33f, 240.22f, -43.10f, 6.20f };
            uc.cx = 1633.33f; uc.cy = 240.22f; uc.cz = -43.10f; uc.radius = 450.0f;
            m_cities.push_back(uc);
        }

        // 6. Thunder Bluff (Horde, Map 1, Area 1638)
        {
            CityInfo tb;
            tb.id = CITY_THUNDER_BLUFF;
            tb.name = "Thunder Bluff";
            tb.teamId = 1;
            tb.mapId = 1;
            tb.areaIds = { 1638 };
            tb.landing = { 1, -1277.37f, 117.21f, 131.00f, 5.24f };
            tb.cx = -1277.37f; tb.cy = 117.21f; tb.cz = 131.00f; tb.radius = 500.0f;
            m_cities.push_back(tb);
        }

        // 7. Sparkwater Port (Horde, Turtle custom Goblin port/capital, Map 1, Area 5225/custom)
        {
            CityInfo sp;
            sp.id = CITY_SPARKWATER;
            sp.name = "Sparkwater";
            sp.teamId = 1;
            sp.mapId = 1;
            sp.areaIds = { 5225, 2041 };
            sp.landing = { 1, -1190.00f, -3740.00f, 12.50f, 3.14f };
            sp.cx = -1190.00f; sp.cy = -3740.00f; sp.cz = 12.50f; sp.radius = 400.0f;
            m_cities.push_back(sp);
        }

        // Outdoor regions definitions
        m_regions.clear();
        auto addReg = [this](uint32 regId, const char* name, uint32 mapId, uint32 zoneId,
                             std::unordered_set<uint32> areaIds, uint8 teamAcc,
                             uint8 minLvl, uint8 maxLvl, float wt)
        {
            OutdoorRegion r;
            r.regionId = regId;
            r.name = name;
            r.mapId = mapId;
            r.zoneId = zoneId;
            r.areaIds = areaIds;
            r.teamAccess = teamAcc;
            r.minLevel = minLvl;
            r.maxLevel = maxLvl;
            r.weight = wt;
            m_regions.push_back(r);
        };

        // Alliance leveling zones
        addReg(1,  "Elwynn Forest",       0, 12,  {12},  0, 1,  10, 1.0f);
        addReg(2,  "Westfall",            0, 40,  {40},  0, 10, 20, 1.0f);
        addReg(3,  "Loch Modan",          0, 38,  {38},  0, 10, 20, 1.0f);
        addReg(4,  "Darkshore",           1, 148, {148}, 0, 10, 20, 1.0f);
        addReg(5,  "Redridge Mountains",  0, 44,  {44},  0, 15, 25, 1.0f);
        addReg(6,  "Duskwood",            0, 10,  {10},  0, 18, 30, 1.0f);
        addReg(7,  "Wetlands",            0, 11,  {11},  0, 20, 30, 1.0f);
        addReg(8,  "Thalassian Highlands",0, 2040,{2040},0, 1,  10, 1.0f);

        // Horde leveling zones
        addReg(10, "Durotar",             1, 14,  {14},  1, 1,  10, 1.0f);
        addReg(11, "Mulgore",             1, 215, {215}, 1, 1,  10, 1.0f);
        addReg(12, "Tirisfal Glades",     0, 85,  {85},  1, 1,  10, 1.0f);
        addReg(13, "The Barrens",         1, 17,  {17},  1, 10, 25, 1.2f);
        addReg(14, "Silverpine Forest",   0, 130, {130}, 1, 10, 20, 1.0f);
        addReg(15, "Stonetalon Mountains",1, 406, {406}, 1, 15, 25, 1.0f);
        addReg(16, "Blackstone Island",   1, 5225,{5225},1, 1,  10, 1.0f);

        // Contested mid / high zones
        addReg(20, "Ashenvale",           1, 331, {331}, 2, 18, 30, 1.0f);
        addReg(21, "Hillsbrad Foothills", 0, 267, {267}, 2, 20, 30, 1.0f);
        addReg(22, "Thousand Needles",    1, 400, {400}, 2, 25, 35, 1.0f);
        addReg(23, "Arathi Highlands",    0, 45,  {45},  2, 30, 40, 1.0f);
        addReg(24, "Desolace",            1, 405, {405}, 2, 30, 40, 1.0f);
        addReg(25, "Stranglethorn Vale",  0, 33,  {33},  2, 30, 45, 1.2f);
        addReg(26, "Badlands",            0, 3,   {3},   2, 35, 45, 0.9f);
        addReg(27, "Swamp of Sorrows",    0, 8,   {8},   2, 35, 45, 0.9f);
        addReg(28, "Tanaris",             1, 440, {440}, 2, 40, 50, 1.1f);
        addReg(29, "Feralas",             1, 357, {357}, 2, 40, 50, 1.0f);
        addReg(30, "The Hinterlands",     0, 47,  {47},  2, 40, 50, 1.0f);
        addReg(31, "Searing Gorge",       0, 51,  {51},  2, 43, 50, 0.9f);
        addReg(32, "Felwood",             1, 361, {361}, 2, 48, 55, 1.0f);
        addReg(33, "Un'Goro Crater",      1, 490, {490}, 2, 48, 55, 1.0f);
        addReg(34, "Burning Steppes",     0, 46,  {46},  2, 50, 58, 1.0f);
        addReg(35, "Western Plaguelands", 0, 28,  {28},  2, 51, 58, 1.0f);
        addReg(36, "Eastern Plaguelands", 0, 139, {139}, 2, 53, 60, 1.1f);
        addReg(37, "Winterspring",        1, 618, {618}, 2, 55, 60, 1.0f);
        addReg(38, "Silithus",            1, 1377,{1377},2, 55, 60, 1.0f);
    }

    const CityInfo* DestinationCatalog::GetCity(CityId id) const
    {
        if (id < m_cities.size())
            return &m_cities[id];
        return nullptr;
    }

    const CityInfo* DestinationCatalog::FindCityByName(const std::string& name) const
    {
        for (const auto& c : m_cities)
        {
            if (name == c.name)
                return &c;
        }
        return nullptr;
    }

    const CityInfo* DestinationCatalog::GetCityForArea(uint32 mapId, uint32 areaId, float x, float y, float z) const
    {
        for (const auto& c : m_cities)
        {
            if (c.mapId != mapId)
                continue;

            if (c.areaIds.find(areaId) != c.areaIds.end())
                return &c;

            // Geometric sphere check fallback
            float dx = x - c.cx;
            float dy = y - c.cy;
            float dz = z - c.cz;
            if (sqrt(dx * dx + dy * dy + dz * dz) <= c.radius)
                return &c;
        }
        return nullptr;
    }

    bool DestinationCatalog::IsInCity(CityId id, uint32 mapId, uint32 areaId, float x, float y, float z) const
    {
        const CityInfo* city = GetCity(id);
        if (!city || city->mapId != mapId)
            return false;

        if (city->areaIds.find(areaId) != city->areaIds.end())
            return true;

        float dx = x - city->cx;
        float dy = y - city->cy;
        float dz = z - city->cz;
        return sqrt(dx * dx + dy * dy + dz * dz) <= city->radius;
    }

    bool DestinationCatalog::IsInAnyCity(uint32 mapId, uint32 areaId, float x, float y, float z) const
    {
        return GetCityForArea(mapId, areaId, x, y, z) != nullptr;
    }

    const OutdoorRegion* DestinationCatalog::GetRegion(uint32 regionId) const
    {
        for (const auto& r : m_regions)
        {
            if (r.regionId == regionId)
                return &r;
        }
        return nullptr;
    }

    const OutdoorRegion* DestinationCatalog::FindRegionByName(const std::string& name) const
    {
        for (const auto& r : m_regions)
        {
            if (name == r.name)
                return &r;
        }
        return nullptr;
    }

    const OutdoorRegion* DestinationCatalog::GetRegionForLocation(uint32 mapId, uint32 areaId, uint32 zoneId) const
    {
        for (const auto& r : m_regions)
        {
            if (r.mapId != mapId)
                continue;

            if (r.zoneId == zoneId || r.areaIds.find(areaId) != r.areaIds.end())
                return &r;
        }
        return nullptr;
    }

    std::vector<const OutdoorRegion*> DestinationCatalog::GetRegionsForLevelAndFaction(uint8 level, uint8 teamId) const
    {
        std::vector<const OutdoorRegion*> matched;
        for (const auto& r : m_regions)
        {
            // Faction check: teamAccess: 0 = Alliance, 1 = Horde, 2 = Both
            if (r.teamAccess != 2 && r.teamAccess != teamId)
                continue;

            // Level range envelope
            if (level >= r.minLevel && level <= r.maxLevel)
                matched.push_back(&r);
        }
        return matched;
    }
}
