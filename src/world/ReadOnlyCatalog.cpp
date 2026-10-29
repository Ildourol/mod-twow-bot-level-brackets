#include "ReadOnlyCatalog.h"
#include <cstring>

#if defined(_MSC_VER)
#define strcasecmp _stricmp
#endif
#include <cmath>
#include <algorithm>

namespace BotLevelBrackets
{

const char* GetCityName(CityId id)
{
    switch (id)
    {
        case CityId::Stormwind:    return "Stormwind";
        case CityId::Ironforge:    return "Ironforge";
        case CityId::Darnassus:    return "Darnassus";
        case CityId::AlahThalas:   return "Alah'Thalas";
        case CityId::Orgrimmar:    return "Orgrimmar";
        case CityId::Undercity:    return "Undercity";
        case CityId::ThunderBluff: return "Thunder Bluff";
        case CityId::Sparkwater:   return "Sparkwater Port";
        default:                   return "Unknown";
    }
}

ReadOnlyCatalog& ReadOnlyCatalog::Instance()
{
    static ReadOnlyCatalog instance;
    return instance;
}

ReadOnlyCatalog::ReadOnlyCatalog()
{
    // Canonical 8 Turtle WoW racial capitals
    // Alliance: Stormwind, Ironforge, Darnassus, Alah'Thalas (High Elf capital)
    m_cities.push_back({ CityId::Stormwind,   "Stormwind",       0, 1519, {},       0, -8810.0f,   640.0f,   94.0f, 250.0f });
    m_cities.push_back({ CityId::Ironforge,   "Ironforge",       0, 1537, {},       0, -4980.0f,  -940.0f,  501.0f, 200.0f });
    m_cities.push_back({ CityId::Darnassus,   "Darnassus",       1, 1657, {},       0,  9950.0f,  2330.0f, 1330.0f, 200.0f });
    m_cities.push_back({ CityId::AlahThalas,  "Alah'Thalas",      0, 2040, { 5225 }, 0,  3212.6f, -2501.4f,  111.7f, 200.0f });

    // Horde: Orgrimmar, Undercity, Thunder Bluff, Sparkwater Port (Goblin capital)
    m_cities.push_back({ CityId::Orgrimmar,   "Orgrimmar",       1, 1637, {},       1,  1570.0f, -4400.0f,    8.0f, 250.0f });
    m_cities.push_back({ CityId::Undercity,   "Undercity",       0, 1497, {},       1,  1620.0f,   240.0f,   60.0f, 200.0f });
    m_cities.push_back({ CityId::ThunderBluff,"Thunder Bluff",   1, 1638, {},       1, -1290.0f,   150.0f,  130.0f, 200.0f });
    m_cities.push_back({ CityId::Sparkwater,  "Sparkwater Port", 1, 4021, {},       1,   840.0f, -5060.0f,    6.0f, 200.0f });

    // Representative sample of outdoor regions across both continents
    m_regions.push_back({ 1,  "Elwynn Forest",      0, 12,   0, 1,  10 });
    m_regions.push_back({ 2,  "Westfall",           0, 40,   0, 10, 20 });
    m_regions.push_back({ 3,  "Redridge Mountains", 0, 44,   2, 15, 25 });
    m_regions.push_back({ 4,  "Duskwood",           0, 10,   2, 18, 30 });
    m_regions.push_back({ 5,  "Stranglethorn Vale", 0, 33,   2, 30, 45 });
    m_regions.push_back({ 6,  "Durotar",            1, 14,   1, 1,  10 });
    m_regions.push_back({ 7,  "Barrens",            1, 17,   1, 10, 25 });
    m_regions.push_back({ 8,  "Thousand Needles",   1, 400,  2, 25, 35 });
    m_regions.push_back({ 9,  "Tanaris",            1, 440,  2, 40, 50 });
    m_regions.push_back({ 10, "Winterspring",       1, 618,  2, 55, 60 });
}

const CityDefinition* ReadOnlyCatalog::GetCity(CityId id) const
{
    for (const auto& def : m_cities)
    {
        if (def.id == id)
            return &def;
    }
    return nullptr;
}

const CityDefinition* ReadOnlyCatalog::GetCityByName(const std::string& name) const
{
    for (const auto& def : m_cities)
    {
        if (strcasecmp(def.name, name.c_str()) == 0)
            return &def;
    }
    return nullptr;
}

const CityDefinition* ReadOnlyCatalog::FindCityForLocation(uint32_t mapId, uint32_t zoneId,
                                                           uint32_t areaId, float x, float y, float z) const
{
    for (const auto& def : m_cities)
    {
        if (def.mapId != mapId)
            continue;

        if (def.zoneId == zoneId)
            return &def;

        for (uint32_t sub : def.subAreaIds)
        {
            if (zoneId == sub || areaId == sub)
                return &def;
        }

        float dx = x - def.centerX;
        float dy = y - def.centerY;
        float dz = z - def.centerZ;
        if ((dx * dx + dy * dy + dz * dz) <= (def.radius * def.radius))
            return &def;
    }
    return nullptr;
}

const OutdoorRegion* ReadOnlyCatalog::GetRegion(uint32_t regionId) const
{
    for (const auto& reg : m_regions)
    {
        if (reg.regionId == regionId)
            return &reg;
    }
    return nullptr;
}

} // namespace BotLevelBrackets
