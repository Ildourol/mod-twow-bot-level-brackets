#ifndef MOD_TWOW_BOT_LEVEL_BRACKETS_READ_ONLY_CATALOG_H
#define MOD_TWOW_BOT_LEVEL_BRACKETS_READ_ONLY_CATALOG_H

#include <cstdint>
#include <string>
#include <vector>

namespace BotLevelBrackets
{

enum class CityId : uint8_t
{
    None = 0,
    Stormwind = 1,
    Ironforge = 2,
    Darnassus = 3,
    AlahThalas = 4,
    Orgrimmar = 5,
    Undercity = 6,
    ThunderBluff = 7,
    Sparkwater = 8,
    Count = 9
};

struct CityDefinition
{
    CityId id;
    const char* name;
    uint32_t mapId;
    uint32_t zoneId;
    std::vector<uint32_t> subAreaIds;
    uint8_t teamId; // 0 = ALLIANCE, 1 = HORDE
    float centerX;
    float centerY;
    float centerZ;
    float radius;
};

struct OutdoorRegion
{
    uint32_t regionId;
    const char* name;
    uint32_t mapId;
    uint32_t zoneId;
    uint8_t teamId; // 0 = Alliance, 1 = Horde, 2 = Contested
    uint8_t minLevel;
    uint8_t maxLevel;
};

class ReadOnlyCatalog
{
public:
    static ReadOnlyCatalog& Instance();

    const std::vector<CityDefinition>& GetCities() const { return m_cities; }
    const CityDefinition* GetCity(CityId id) const;
    const CityDefinition* GetCityByName(const std::string& name) const;
    const CityDefinition* FindCityForLocation(uint32_t mapId, uint32_t zoneId, uint32_t areaId,
                                            float x, float y, float z) const;

    const std::vector<OutdoorRegion>& GetRegions() const { return m_regions; }
    const OutdoorRegion* GetRegion(uint32_t regionId) const;

private:
    ReadOnlyCatalog();
    std::vector<CityDefinition> m_cities;
    std::vector<OutdoorRegion> m_regions;
};

const char* GetCityName(CityId id);

} // namespace BotLevelBrackets

#endif // MOD_TWOW_BOT_LEVEL_BRACKETS_READ_ONLY_CATALOG_H
